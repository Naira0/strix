#include <iostream>
#include <charconv>

#include "compiler.hpp"
#include "scanner.hpp"
#include "fmt.hpp"
#include "object.hpp"
#include "debug.hpp"

bool Compiler::compile()
{
    m_had_error = false;

    advance();

    while(!match(TokenType::Eof))
        declaration();

    emit_bytes(OpCode::Return);

    return !m_had_error;
}

void Compiler::advance()
{
    m_previous_token = m_current_token;

    if(check(TokenType::FStringStart))
        m_state = ParseState::FString;

    while(true)
    {
        if(m_state == ParseState::FString)
            m_current_token = m_scanner.scan_fstring();
        else
            m_current_token = m_scanner.scan_token();

#if DEBUG_TOKENS
        fmt::print("Token -> {}\n", m_current_token);
#endif

        if(m_scanner.state.ok)
            break;

        error(m_scanner.state.message);
    }
}

void Compiler::expression()
{
    parse_precedence(Precedence::Assignment);
}

void Compiler::consume(TokenType type, std::string_view message)
{
    if(m_current_token.type == type)
        return advance();

    error_at(m_current_token, message);
}

inline void Compiler::error_at(Token &token, std::string_view message)
{
    if(m_panic_mode)
        return;

    m_panic_mode = true;

    fmt::eprint("[{}:{}] error on token '{}'\n\tmessage: {}\n",
               token.line, token.column, token.lexeme, message);

    m_had_error = true;
}

inline void Compiler::error(std::string_view message)
{
    error_at(m_previous_token, message);
}

inline Compiler::ParseRule Compiler::get_rule(TokenType kind) const
{
    return m_rules[(uint8_t)kind];
}

template<typename ...A>
inline void Compiler::emit_bytes(A ...a)
{
    (m_current_chunk.bytes.emplace_back(a, m_current_token.line), ...);
}

size_t Compiler::emit_jmp(OpCode instruction)
{
    auto &chunk = m_current_chunk;

    chunk.set(instruction, Value(0.0), m_previous_token.line);

    return chunk.bytes.size() - 1;
}

void Compiler::patch_jmp(size_t offset)
{
    auto &bytes = m_current_chunk.bytes;
    auto &value = m_current_chunk.constants[bytes[offset].constant];

    double jmp = bytes.size() - offset - 1;

    value = Value(jmp);
}

void Compiler::emit_rollback(size_t start)
{
    double amount = m_current_chunk.bytes.size() - start - 1;
    m_current_chunk.set(OpCode::RollBack, Value(amount), m_current_token.line);
}

inline void Compiler::number()
{
    double value;

    std::string_view lexeme = m_previous_token.lexeme;

    std::from_chars(lexeme.data(), lexeme.data()+lexeme.size(), value);

    emit_constant(Value(value));
}

inline void Compiler::string()
{
    if(String::intern_strings.contains(m_current_token.lexeme))
        return;
    emit_constant(new String(m_previous_token.lexeme));
}

void Compiler::fstring()
{
    m_state = ParseState::FString;

    emit_constant(Value(new String(std::string_view{""})));

    while(!check(TokenType::FStringEnd))
    {
        expression();

        if(m_previous_token.type != TokenType::String)
            emit_bytes(OpCode::ToString);

        emit_bytes(OpCode::Add);
    }

    m_state = ParseState::None;

    advance();
}

inline void Compiler::emit_constant(Value &&value)
{
    m_current_chunk.set_constant(std::forward<Value>(value), m_current_token.line);
}

inline void Compiler::grouping()
{
    expression();
    consume(TokenType::RightParen, "Expected ')' after expression");
}

inline void Compiler::binary()
{
    TokenType operator_type = m_previous_token.type;
    ParseRule rule = get_rule(operator_type);

    parse_precedence((Precedence)((uint8_t)rule.precedence+1));

    switch(operator_type)
    {
        case TokenType::BangEqual:    return emit_bytes(OpCode::Equal, OpCode::Not);
        case TokenType::EqualEqual:   return emit_bytes(OpCode::Equal);
        case TokenType::Greater:      return emit_bytes(OpCode::Greater);
        case TokenType::GreaterEqual: return emit_bytes(OpCode::Not);
        case TokenType::Less:         return emit_bytes(OpCode::Less);
        case TokenType::LessEqual:    return emit_bytes(OpCode::Greater, OpCode::Not);
        case TokenType::Plus:         return emit_bytes(OpCode::Add);
        case TokenType::Minus:        return emit_bytes(OpCode::Subtract);
        case TokenType::Star:         return emit_bytes(OpCode::Multiply);
        case TokenType::Percent:      return emit_bytes(OpCode::Mod);
        case TokenType::Slash:        return emit_bytes(OpCode::Divide);
        case TokenType::Or:           return emit_bytes(OpCode::Or);
        case TokenType::And:          return emit_bytes(OpCode::And);
        default: return;
    }
}

void Compiler::unary()
{
    TokenType operator_type = m_previous_token.type;

    parse_precedence(Precedence::Unary);

    switch(operator_type)
    {
        case TokenType::Minus: return emit_bytes(OpCode::Negate);
        case TokenType::Bang:  return emit_bytes(OpCode::Not);
        default: return;
    }
}

inline void Compiler::literal()
{
    switch(m_previous_token.type)
    {
        case TokenType::True:  return emit_bytes(OpCode::True);
        case TokenType::False: return emit_bytes(OpCode::False);
        case TokenType::Nil:   return emit_bytes(OpCode::Nil);
        default: return;
    }
}

void Compiler::variable()
{
    std::string_view identifier = m_previous_token.lexeme;

    int scope_depth = resolve_scope();

    if(scope_depth == -1)
        return error("use of undeclared variable");

    Variable var = m_variables[scope_depth][identifier];

    Token previous = m_previous_token;

    emit_constant(Value((double)var.index));

    if(m_can_assign && match(TokenType::Equal))
    {
        if(!var.is_mutable)
            return error_at(previous, "constant variable cannot be reassigned");

        expression();
        emit_bytes(var.depth == 0 ? OpCode::SetStatic : OpCode::SetVar);
    }
    else
        emit_bytes(var.depth == 0 ? OpCode::GetStatic : OpCode::GetVar);
}

inline void Compiler::begin_scope()
{
    m_scope_depth++;
    m_variables.emplace_back();
}

void Compiler::end_scope()
{
    if(m_scope_depth == 0)
        return;

    m_scope_depth--;
    m_var_index -= m_variables[m_scope_depth].size();
    m_variables.pop_back();
}

void Compiler::block()
{
    while(!check(TokenType::RightBrace) && !check(TokenType::Eof))
        declaration();

    consume(TokenType::RightBrace, "expected '}' at the end of block");
}

void Compiler::statement()
{
    using enum class TokenType;

    if(match(Print))
        print_stmt();
    else if(match(If))
        if_stmt();
    else if(match(While))
        while_stmt();
    else if(match(LeftBrace))
    {
        begin_scope();
        block();
        end_scope();
    }
    else
        expression();
}

void Compiler::if_stmt()
{
    expression();

    size_t if_jmp = emit_jmp(OpCode::Jif);

    statement();

    size_t else_jmp = emit_jmp(OpCode::Jump);

    patch_jmp(if_jmp);

    if(match(TokenType::Else))
        statement();

    patch_jmp(else_jmp);
}

void Compiler::if_expr()
{
    expression();

    size_t if_jmp = emit_jmp(OpCode::Jif);

    consume(TokenType::Do, "expected do keyword after if condition");

    expression();

    size_t else_jmp = emit_jmp(OpCode::Jump);

    patch_jmp(if_jmp);

    consume(TokenType::Else, "must have a matching else with an if expression");

    expression();

    patch_jmp(else_jmp);
}

void Compiler::while_stmt()
{
    size_t start = m_current_chunk.bytes.size()-2;

    expression();

    size_t jmp = emit_jmp(OpCode::Jif);

    statement();

    emit_rollback(start);

    patch_jmp(jmp);
}

void Compiler::declaration()
{
    if(match(TokenType::Var) || match(TokenType::Const))
        var_declaration();
    else
        statement();

    if(m_panic_mode)
        synchronize();
}

void Compiler::var_declaration()
{
    bool is_const = m_previous_token.type == TokenType::Const;
    int index = m_scope_depth == 0 ? m_static_index++ : m_var_index;

    Variable var
    {
        .depth = m_scope_depth,
        .is_mutable = !is_const,
        .index      = index,
    };

    emit_constant(Value((double)index));

    parse_variable(var);

    if(match(TokenType::Equal))
        expression();
    else
    {
        if(is_const)
            return error("constant variable must be initialized with a value");

        emit_bytes(OpCode::Nil);
    }

    emit_bytes(m_scope_depth == 0 ? OpCode::SetStatic : OpCode::SetVar);

    if(match(TokenType::Comma))
        var_declaration();
}

int Compiler::resolve_scope()
{
    std::string_view identifier = m_previous_token.lexeme;

    for(int i = m_scope_depth; i >= 0; i--)
    {
        if(m_variables[i].contains(identifier))
            return i;
    }

    return -1;
}

void Compiler::parse_variable(Variable var)
{
    consume(TokenType::Identifier, "expected identifier");

    std::string_view var_name = m_previous_token.lexeme;

    VarTable &vars = m_variables[m_scope_depth];

    if(vars.contains(var_name))
        return error("variable is already defined at this scope");

    vars.emplace(var_name, var);
}

void Compiler::synchronize()
{
    m_panic_mode = false;

    while(m_current_token.type != TokenType::Eof)
    {
        auto current = m_current_token.type;

        if(current > TokenType::Or && current < TokenType::Error)
            return;

        advance();
    }
}

inline void Compiler::print_stmt()
{
    expression();
    emit_bytes(OpCode::Print);
}

inline void Compiler::parse_precedence(Precedence precedence)
{
    advance();

    ParseFN prefix_rule = get_rule(m_previous_token.type).prefix;

    if(prefix_rule == nullptr)
        return error("expected expression");

    bool can_assign = precedence <= Precedence::Assignment;

    m_can_assign = can_assign;

    call(prefix_rule);

    if(match(TokenType::Eof))
        return;

    while(precedence <= get_rule(m_current_token.type).precedence)
    {
        advance();

        ParseFN infix_rule = get_rule(m_previous_token.type).infix;

        if(infix_rule == nullptr)
            return;

        call(infix_rule);
    }

    if(can_assign && match(TokenType::Equal))
        error("invalid assignment");
}

inline void Compiler::call(Compiler::ParseFN fn)
{
    (this->*fn)();
}

inline bool Compiler::check(TokenType type) const
{
    return m_current_token.type == type;
}

bool Compiler::match(TokenType type)
{
    if(!check(type))
        return false;
    advance();
    return true;
}

const Compiler::ParseRule Compiler::m_rules[] =
{
        {&Compiler::grouping, nullptr, Precedence::None}, //leftparen
        {nullptr,     nullptr,   Precedence::None}, // rightparen
        {nullptr,     nullptr,   Precedence::None}, // leftbrace
        {nullptr,     nullptr,   Precedence::None}, // rightbrace
        {nullptr,     nullptr,   Precedence::None}, // comma
        {nullptr,     nullptr,   Precedence::None}, // dot
        {&Compiler::unary, &Compiler::binary,   Precedence::Term}, // minus
        {nullptr, &Compiler::binary,   Precedence::Term}, // plus
        {nullptr,     nullptr,   Precedence::None}, // semicolon
        {nullptr, &Compiler::binary,   Precedence::Factor}, // slash
        {nullptr, &Compiler::binary,   Precedence::Factor}, // star
        {nullptr, &Compiler::binary,   Precedence::Factor}, // percent
        {&Compiler::unary,     nullptr,   Precedence::None}, // bang
        {nullptr, &Compiler::binary,   Precedence::Equality}, // bangequal
        {nullptr,     nullptr,   Precedence::None}, // equal
        {nullptr, &Compiler::binary,   Precedence::Comparison}, // equalequal
        {nullptr, &Compiler::binary,   Precedence::Comparison}, // greater
        {nullptr, &Compiler::binary,   Precedence::Comparison}, // greaterequal
        {nullptr, &Compiler::binary,   Precedence::Comparison}, // less
        {nullptr, &Compiler::binary,   Precedence::Comparison}, // lessequal
        {&Compiler::variable,     nullptr,   Precedence::None}, // identifier
        {&Compiler::string,     nullptr,     Precedence::None}, // string
        {&Compiler::fstring, nullptr,        Precedence::None}, // fstringstart
        {nullptr, nullptr, Precedence::None}, // fstringend
        {&Compiler::number,     nullptr,     Precedence::None}, // number
        {nullptr,     &Compiler::binary,   Precedence::And}, //  and
        {nullptr,     nullptr,   Precedence::None}, // else
        {&Compiler::literal,     nullptr,   Precedence::None}, // false
        {&Compiler::literal,     nullptr,   Precedence::None}, // true
        {&Compiler::literal,     nullptr,   Precedence::None}, // nil
        {nullptr,     nullptr,   Precedence::None}, // do
        {nullptr,     &Compiler::binary,   Precedence::Or}, // or
        {nullptr,     nullptr,   Precedence::None}, // obj
        {nullptr,     nullptr,   Precedence::None}, // for
        {&Compiler::if_expr,     nullptr,   Precedence::None}, // if
        {nullptr,     nullptr,   Precedence::None}, // fn
        {nullptr,     nullptr,   Precedence::None}, // print
        {nullptr,     nullptr,   Precedence::None}, // return
        {nullptr,     nullptr,   Precedence::None}, // super
        {nullptr,     nullptr,   Precedence::None}, // this
        {nullptr,     nullptr,   Precedence::None}, // var
        {nullptr,     nullptr,   Precedence::None}, // const
        {nullptr,     nullptr,   Precedence::None}, // while
        {nullptr,     nullptr,   Precedence::None}, // error
        {nullptr,     nullptr,   Precedence::None}, // eof
};




