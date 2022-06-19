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
    (m_chunk.bytes.emplace_back(a, m_current_token.line), ...);
}

size_t Compiler::emit_jmp(OpCode instruction)
{
    auto &chunk = m_chunk;

    chunk.set(instruction, Value(0.0), m_previous_token.line);

    return chunk.bytes.size() - 1;
}

void Compiler::patch_jmp(size_t offset)
{
    auto &bytes = m_chunk.bytes;
    auto &value = m_chunk.constants[bytes[offset].constant];

    double jmp = bytes.size() - offset - 1;

    value = Value(jmp);
}

void Compiler::emit_rollback(size_t start)
{
    double amount = m_chunk.bytes.size() - start - 1;
    m_chunk.set(OpCode::RollBack, Value(amount), m_current_token.line);
}

bool Compiler::emit_cache()
{
    if(m_cache.items.empty())
        return false;

    Value value = m_cache.get().value;

    emit_constant(std::move(value));

    return true;
}

bool Compiler::is_known() const
{
    if(m_cache.items.empty())
        return false;
    const Cache::Item &item = m_cache.items.back();
    return is_known(item);
}

bool Compiler::is_two_known() const
{
    if(m_cache.items.size() < 2)
        return false;
    const auto &item1 = m_cache.items.back();
    const auto &item2 = m_cache.items.end()-1;
    return is_known(item1) && is_known(*item2);
}

bool Compiler::is_known(const Cache::Item &item) const
{
    if(item.type == Cache::Type::Value)
        return true;

    int depth = resolve_var(item.lexeme);

    if(depth != -1 && m_variables[depth].contains(item.lexeme))
    {
        Variable var = m_variables[depth].at(item.lexeme);

        if(var.value_known)
            return true;
    }

    return false;
}

inline void Compiler::number()
{
    double value;

    std::string_view lexeme = m_previous_token.lexeme;

    std::from_chars(lexeme.data(), lexeme.data()+lexeme.size(), value);

    m_cache.set(Value(value));
}

inline void Compiler::string()
{
    if(String::intern_strings.contains(m_current_token.lexeme))
        return;

    m_cache.set(new String(m_previous_token.lexeme));
}

void Compiler::fstring()
{
    m_state = ParseState::FString;

    emit_constant(new String(std::string_view{""}));

    while(!check(TokenType::FStringEnd))
    {
        expression();

        emit_cache();

        if(m_previous_token.type != TokenType::String)
            emit_bytes(OpCode::ToString);

        emit_bytes(OpCode::Add);
    }

    m_state = ParseState::None;

    advance();
}

inline void Compiler::emit_constant(Value &&value)
{
    m_chunk.set_constant(std::forward<Value>(value), m_current_token.line);
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

    if(is_two_known())
    {
        auto b = m_cache.get().value;
        auto a = m_cache.get().value;

        try
        {
            switch(operator_type)
            {
                case TokenType::EqualEqual:   return m_cache.set(Value(a == b));
                case TokenType::Greater:      return m_cache.set(Value(a > b));
                case TokenType::GreaterEqual: return m_cache.set(Value(a >= b));
                case TokenType::Less:         return m_cache.set(Value(a < b));
                case TokenType::LessEqual:    return m_cache.set(Value(a <= b));
                case TokenType::Plus:         return m_cache.set(a + b);
                case TokenType::Minus:        return m_cache.set(a - b);
                case TokenType::Star:         return m_cache.set(a * b);
                case TokenType::Caret:        return m_cache.set(a.power(b));
                case TokenType::Percent:      return m_cache.set(a.mod(b));
                case TokenType::Slash:        return m_cache.set(a / b);
                case TokenType::Or:           return m_cache.set(Value(!a.is_falsy() || !b.is_falsy()));
                case TokenType::And:          return m_cache.set(Value(!a.is_falsy() && !b.is_falsy()));
                default: return;
            }
        }
        catch(std::exception &e)
        {
            return error(e.what());
        }
    }

    emit_cache();
    emit_cache();

    switch(operator_type)
    {
        case TokenType::BangEqual:    return emit_bytes(OpCode::Cmp, OpCode::Not);
        case TokenType::EqualEqual:   return emit_bytes(OpCode::Cmp);
        case TokenType::Greater:      return emit_bytes(OpCode::Greater);
        case TokenType::GreaterEqual: return emit_bytes(OpCode::Greater, OpCode::Not);
        case TokenType::Less:         return emit_bytes(OpCode::Less);
        case TokenType::LessEqual:    return emit_bytes(OpCode::Greater, OpCode::Not);
        case TokenType::Plus:         return emit_bytes(OpCode::Add);
        case TokenType::Minus:        return emit_bytes(OpCode::Subtract);
        case TokenType::Star:         return emit_bytes(OpCode::Multiply);
        case TokenType::Caret:        return emit_bytes(OpCode::Power);
        case TokenType::Percent:      return emit_bytes(OpCode::Mod);
        case TokenType::Slash:        return emit_bytes(OpCode::Divide);
        case TokenType::Or:           return emit_bytes(OpCode::Or);
        case TokenType::And:          return emit_bytes(OpCode::And);
        case TokenType::Is:           return emit_bytes(OpCode::TypeCmp);
        default: return;
    }

}

void Compiler::unary()
{
    TokenType operator_type = m_previous_token.type;

    parse_precedence(Precedence::Unary);

    if(is_known())
    {
        Value value = m_cache.get().value;

        switch(operator_type)
        {
            case TokenType::Minus:
            {
                if(value.type != ValueType::Number)
                    return error("negation value must be an integral type");

                value.as.number = -value.as.number;

                return m_cache.set(std::move(value));
            }
            case TokenType::Bang: return m_cache.set(Value(value.is_falsy()));
            default: return;
        }
    }

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
        case TokenType::True:  return m_cache.set(Value(true));
        case TokenType::False: return m_cache.set(Value(false));
        case TokenType::Nil:   return m_cache.set(Value(nullptr));
        default: return;
    }
}

void Compiler::variable()
{
    std::string_view identifier = m_previous_token.lexeme;

    int scope_depth = resolve_var(identifier);

    if(scope_depth == -1)
        return error("use of undeclared variable");

    Variable var = m_variables[scope_depth][identifier];

    Token previous_token = m_previous_token;

    OpCode op;
    OpCode extra = OpCode::NoOp;

    // will be set to false if it was a get op otherwise it will be true and will throw an error if the var is immutable
    bool assigned = true;
    // will determine if it the variable should be pushed on to the stack at the end of the function
    bool get_mem = true;

    using enum class TokenType;

    if(m_can_assign && match(Equal))
    {
        expression();
        emit_cache();
        op = OpCode::SetMem;
    }
    else if(m_current_token.type > Star && m_current_token.type < Caret)
    {
        op = OpCode::LoadAddr;
        extra = mod_assignable(var, get_mem);
    }
    else
    {
        assigned = false;
        get_mem  = false;
        op = OpCode::GetMem;
    }

    if(!var.is_mutable && assigned)
        return error_at(previous_token, "constant variable cannot be reassigned");

    m_chunk.set(op, Value(var.index), previous_token.line);

    if(assigned)
        var.type = m_chunk.constants.back().type;

    if(extra != OpCode::NoOp)
        emit_bytes(extra);
    if(get_mem)
        m_chunk.set(OpCode::GetMem, Value(var.index), previous_token.line);
}

OpCode Compiler::mod_assignable(Variable var, bool &get_mem)
{
    using enum class TokenType;

    OpCode op = OpCode::NoOp;

    if(match(PlusEqual))
        op = OpCode::Add;
    else if(match(MinusEqual))
        op = OpCode::Subtract;
    else if(match(SlashEqual))
        op = OpCode::Divide;
    else if(match(StarEqual))
        op = OpCode::Multiply;

    if(match(PlusPlus) || match(MinusMinus))
    {
        op = m_previous_token.type == PlusPlus ? OpCode::Increment : OpCode::Decrement;
        get_mem = false;
        m_chunk.set(OpCode::GetMem, Value(var.index), m_previous_token.line);
    }
    else
    {
        expression();
        emit_cache();
    }

    return op;
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
    m_data_index -= m_variables[m_scope_depth].size();
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
    else if(match(Switch))
        switch_stmt();
    else if(match(LeftBrace))
    {
        begin_scope();
        block();
        end_scope();
    }
    else if(match(For))
        for_stmt();
    else if(match(SemiColon))
        return;
    else if(match(Continue) || match(Break))
        continue_break_stmt();
    else
        expression();
}

void Compiler::continue_break_stmt()
{
    if(m_loop_jmps.empty())
        return error("break statement cannot be used outside of a loop");

    OpCode op = check_last(TokenType::Break) ? OpCode::Jump : OpCode::RollBack;
    size_t jmp = -1;

    if(op == OpCode::RollBack)
        emit_rollback(m_loop_starts[m_loop_jmps.size()-1]);
    else
        jmp = emit_jmp(op);

    m_loop_jmps[m_loop_jmps.size()-1].emplace_back(jmp);
}

void Compiler::if_stmt()
{
    expression();
    emit_cache();

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
    emit_cache();

    size_t if_jmp = emit_jmp(OpCode::Jif);

    consume(TokenType::Do, "expected do keyword after if condition");

    expression();
    emit_cache();

    size_t else_jmp = emit_jmp(OpCode::Jump);

    patch_jmp(if_jmp);

    consume(TokenType::Else, "must have a matching else with an if expression");

    expression();
    emit_cache();


    patch_jmp(else_jmp);
}

void Compiler::while_stmt()
{
    m_loop_jmps.emplace_back();

    size_t start = m_chunk.bytes.size() - 2;
    m_loop_starts[m_loop_jmps.size()-1] = start;

    expression();
    emit_cache();

    size_t jmp = emit_jmp(OpCode::Jif);

    statement();

    emit_rollback(start);

    patch_jmp(jmp);

    for(auto i : m_loop_jmps[m_loop_jmps.size()-1])
    {
        if(i != -1)
            patch_jmp(i);
    }

    m_loop_jmps.pop_back();
}

void Compiler::switch_stmt()
{
    begin_scope();

    // switch value
    expression();
    emit_cache();

    consume(TokenType::LeftBrace, "expected token '{' after switch value");

    // for all the exit jumps after the end of every branch
    std::vector<size_t> jmp_table;

    while(!check(TokenType::RightBrace) && !check(TokenType::Eof))
    {
        // default label
        if(match(TokenType::Default))
        {
            consume(TokenType::Colon, "expected token ':' after case value");

            statement();

            if(!check(TokenType::RightBrace))
                return error("default label must be the last case in switch statement");

            break;
        }

        // case value
        expression();
        emit_cache();

        consume(TokenType::Colon, "expected token ':' after case value");

        emit_bytes(OpCode::Cmp);

        size_t jmp = emit_jmp(OpCode::Jif);

        // body
        statement();

        jmp_table.push_back(emit_jmp(OpCode::Jump));

        patch_jmp(jmp);

        // pops case value
        emit_bytes(OpCode::Pop);
    }

    for(size_t jmp : jmp_table)
        patch_jmp(jmp);

    // pops switch value
    emit_bytes(OpCode::Pop);

    end_scope();
    consume(TokenType::RightBrace, "expected token '}' at the end of switch statement");
}

void Compiler::for_stmt()
{
    m_loop_jmps.emplace_back();

    begin_scope();

    size_t body_start, body_jmp, inc_start, exit_jmp;
    bool range_based = false;
    uint16_t index = -1;
    size_t &line = m_previous_token.line;

    // initializer clause
    if(check(TokenType::Identifier))
    {
        auto identifier = m_current_token.lexeme;

        if(match(TokenType::Identifier) && match(TokenType::In))
        {
            Variable var = build_var(true);

            index = var.index;

            expression();
            emit_cache();

            set_var(var, identifier);

            m_chunk.set(OpCode::SetMem, Value(var.index), line);

            consume(TokenType::DotDot, "expected token '..'");

            bool inclusive = match(TokenType::Equal);

            body_start = m_chunk.bytes.size()-2;
            m_loop_starts[m_loop_jmps.size()-1] = body_start;

            // gets index
            m_chunk.set(OpCode::GetMem, Value(var.index), line);

            // gets end range
            expression();
            emit_cache();

            // condition
            if(inclusive)
                emit_bytes(OpCode::Greater, OpCode::Not);
            else
                emit_bytes(OpCode::Less);

            exit_jmp = emit_jmp(OpCode::Jif);

            range_based = true;

            goto body;
        }
        else
            var_declaration(false);
    }
    else
    {
        expression();
        emit_cache();
    }

#define CONSUME consume(TokenType::SemiColon, "expected ';'");

    CONSUME

    body_start = m_chunk.bytes.size()-2;

    // condition clause

    expression();
    emit_cache();

    m_loop_starts[m_loop_jmps.size()-1] = m_chunk.bytes.size();

    CONSUME

    exit_jmp = emit_jmp(OpCode::Jif);

    // increment clause
    body_jmp = emit_jmp(OpCode::Jump);
    inc_start = m_chunk.bytes.size()-1;

    expression();
    emit_cache();

    patch_jmp(body_jmp);

    emit_rollback(body_start);

    body_start = inc_start;

    patch_jmp(body_start);

body:

    // body
    statement();

    if(range_based)
    {
        m_chunk.set(OpCode::LoadAddr, Value(index), line);
        emit_bytes(OpCode::Increment);
    }

    emit_rollback(body_start);

    if(exit_jmp != -1)
        patch_jmp(exit_jmp);

    for(auto i : m_loop_jmps[m_loop_jmps.size()-1])
    {
        if(i != -1)
            patch_jmp(i);
    }

    end_scope();

    m_loop_jmps.pop_back();

#undef CONSUME
}

void Compiler::declaration()
{
    if(match(TokenType::Var) || match(TokenType::Const))
        var_declaration(true);
    else
        statement();

    if(m_panic_mode)
        synchronize();
}

void Compiler::var_declaration(bool consume_identifier = true)
{
    bool is_const = m_previous_token.type == TokenType::Const;
    uint16_t index = m_data_index++;

    Variable var
    {
        .depth      = m_scope_depth,
        .is_mutable = !is_const,
        .index      = index,
    };

    if(consume_identifier)
        consume(TokenType::Identifier, "expected identifier");

    std::string_view var_name = m_previous_token.lexeme;

    if(match(TokenType::Equal))
    {
        expression();

        if(is_known())
            var.value_known = true;

        emit_cache();
    }
    else
    {
        if(is_const)
            return error("constant variable must be initialized with a value");

        emit_constant(Value(nullptr));
    }

    Value &value = m_chunk.constants.back();
    var.value = &value;
    var.type = value.type;

    m_chunk.set(OpCode::SetMem, Value(index), m_previous_token.line);

    set_var(var, var_name);

    if(match(TokenType::Comma))
        var_declaration(false);
}

int Compiler::resolve_var(std::string_view identifier) const
{
    for(int i = m_scope_depth; i >= 0; i--)
    {
        if(m_variables[i].contains(identifier))
            return i;
    }

    return -1;
}

void Compiler::set_var(Variable var, std::string_view var_name)
{
    VarTable &vars = m_variables[m_scope_depth];

    if(vars.contains(var_name))
        return error("variable is already defined in this scope");

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
    emit_cache();
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

bool Compiler::check_last(TokenType type) const
{
    return m_previous_token.type == type;
}

bool Compiler::match(TokenType type)
{
    if(!check(type))
        return false;
    advance();
    return true;
}

Compiler::Variable Compiler::build_var(bool is_mutable)
{
    return Variable
    {
        .depth = m_scope_depth,
        .is_mutable = is_mutable,
        .index = m_data_index++
    };
}

const Compiler::ParseRule Compiler::m_rules[] =
{
        {&Compiler::grouping, nullptr, Precedence::None}, //leftparen
        {nullptr,     nullptr,   Precedence::None}, // rightparen
        {nullptr,     nullptr,   Precedence::None}, // leftbrace
        {nullptr,     nullptr,   Precedence::None}, // rightbrace
        {nullptr,     nullptr,   Precedence::None}, // comma
        {nullptr,     nullptr,   Precedence::None}, // dot
        {nullptr,     nullptr,   Precedence::None}, // dotdot
        {&Compiler::unary, &Compiler::binary,   Precedence::Term}, // minus
        {nullptr, &Compiler::binary,   Precedence::Term}, // plus
        {nullptr,     nullptr,   Precedence::None}, // semicolon
        {nullptr,     nullptr, Precedence::None},  // colon
        {nullptr, &Compiler::binary,   Precedence::Factor}, // slash
        {nullptr, &Compiler::binary,   Precedence::Factor}, // star
        {nullptr, nullptr, Precedence::None}, // plus equal
        {nullptr, nullptr, Precedence::None}, // minus equal
        {nullptr, nullptr, Precedence::None}, // star equal
        {nullptr, nullptr, Precedence::None}, // slash equal
        {nullptr, nullptr, Precedence::None}, // plus plus
        {nullptr, nullptr, Precedence::None}, // minus minus
        {nullptr, &Compiler::binary, Precedence::Primary}, // caret
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
        {nullptr,     &Compiler::binary,   Precedence::And}, // is
        {nullptr, nullptr, Precedence::None}, // in
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
        {nullptr,     nullptr,   Precedence::None}, // switch
        {nullptr,     nullptr,   Precedence::None}, // continue
        {nullptr,     nullptr,   Precedence::None}, // break
        {nullptr,     nullptr,   Precedence::None}, // default
        {nullptr,     nullptr,   Precedence::None}, // error
        {nullptr,     nullptr,   Precedence::None}, // eof
};