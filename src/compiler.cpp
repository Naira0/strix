#include <iostream>
#include <charconv>

#include "compiler.hpp"
#include "scanner.hpp"
#include "util/fmt.hpp"
#include "util/debug.hpp"
#include "objects/string.hpp"
#include "objects/tuple.hpp"

std::optional<Function> Compiler::compile()
{
    advance();

    while(!match(TokenType::Eof) && !m_had_error)
        declaration();

    if(m_had_error)
        return std::nullopt;

    // entry name would only be set if it is found so this should always be valid
    if(!m_entry_fn.name.empty())
    {
        emit_byte(OpCode::Constant, new Function(std::move(m_entry_fn)));
        emit_byte(OpCode::Call, 0.0);
    }

    emit_bytes(OpCode::Return);

    Function fn("static chunk");

    fn.chunk = std::move(m_static_chunk);

    return fn;
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
    (current_chunk().bytes.emplace_back(a, m_current_token.line), ...);
}

template<typename T>
void Compiler::emit_byte(OpCode code, T &&value)
{
    static_assert(std::is_constructible_v<Value, T>, "wrong type for value");

    current_chunk().set(code, std::forward<Value>(Value(value)), m_previous_token.line);
}

size_t Compiler::emit_jmp(OpCode instruction)
{
    auto &chunk = current_chunk();

    emit_byte(instruction, 0.0);

    return chunk.bytes.size() - 1;
}

void Compiler::patch_jmp(size_t offset)
{
    auto &bytes = current_chunk().bytes;
    auto &value = current_chunk().constants[bytes[offset].constant];

    double jmp = bytes.size() - offset - 1;

    value = Value(jmp);
}

void Compiler::emit_rollback(size_t start)
{
    double amount = current_chunk().bytes.size() - start - 1;
    emit_byte(OpCode::RollBack, amount);
}

inline void Compiler::number()
{
    double value;

    std::string_view lexeme = m_previous_token.lexeme;

    std::from_chars(lexeme.data(), lexeme.data()+lexeme.size(), value);

    emit_byte(OpCode::Constant, value);
}

inline void Compiler::string()
{
    if(String::intern_strings.contains(m_current_token.lexeme))
        return;

    emit_byte(OpCode::Constant, new String(m_previous_token.lexeme));
}

void Compiler::fstring()
{
    m_state = ParseState::FString;

    emit_byte(OpCode::Constant, new String(std::string_view{""}));

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

void Compiler::identifier()
{
    std::string_view identifier = m_previous_token.lexeme;

    int scope_depth = resolve_var(identifier);

    if(scope_depth == -1)
        return error("use of unknown identifier");

    Identifier id = m_identifiers[scope_depth][identifier];


    if(id.index() == 2)
    {
        match(TokenType::LeftParen);
        uint8_t arg_count = _call();

        auto native_fn = std::get<NativeFunction>(id);
        emit_byte(OpCode::Constant, new NativeFunction(native_fn));

        emit_byte(OpCode::Call, (double)arg_count);

        return;
    }

    if(match(TokenType::LeftParen))
    {
        uint16_t index = id_index(id);

        uint8_t arg_count = _call();

        emit_byte(OpCode::GetMem, index);

        emit_byte(OpCode::Call, (double)arg_count);

        return;
    }

    if(id.index() == 1)
    {
        FunctionData fn_data = std::get<FunctionData>(id);

        emit_byte(OpCode::GetMem, fn_data.index);

        return;
    }

    auto var = std::get<Variable>(id);

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

    emit_byte(op, var.index);

    if(extra != OpCode::NoOp)
        emit_bytes(extra);
    if(get_mem)
        emit_byte(OpCode::GetMem, var.index);
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
        emit_byte(OpCode::GetMem, var.index);
    }
    else
        expression();

    return op;
}

inline void Compiler::begin_scope()
{
    m_scope_depth++;
    m_identifiers.emplace_back();
}

void Compiler::end_scope()
{
    if(m_scope_depth == 0)
        return;

    m_scope_depth--;
    m_data_index -= m_identifiers[m_scope_depth].size();
    m_identifiers.pop_back();
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

    // disallows top level statements
//    if(m_scope_depth == 0)
//        return error("statements cannot be used at a global scope");

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
    else if(match(Return))
        return_stmt();
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
    m_loop_jmps.emplace_back();

    size_t start = current_chunk().bytes.size() - 2;
    m_loop_starts[m_loop_jmps.size()-1] = start;

    expression();

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

    uint16_t data_index = m_data_index++;

    emit_byte(OpCode::SetMem, data_index);

    consume(TokenType::LeftBrace, "expected token '{' after switch value");

    // for all the exit jumps after the end of every branch
    std::vector<size_t> jmp_table;

    int default_label = -1;

    while(!check(TokenType::RightBrace) && !check(TokenType::Eof))
    {
        // default label
        if(match(TokenType::Default))
        {
            consume(TokenType::Colon, "expected token ':' after case value");

            if(default_label != -1)
                return error("default label has been previously defined");

            // jumps past default label body
            size_t jmp = emit_jmp(OpCode::Jump);

            default_label = current_chunk().bytes.size()-2;

            statement();

            // jumps out of switch statement
            jmp_table.push_back(emit_jmp(OpCode::Jump));

            patch_jmp(jmp);

            continue;
        }

        emit_byte(OpCode::GetMem, data_index);

        // case value
        expression();

        consume(TokenType::Colon, "expected token ':' after case value");

        emit_bytes(OpCode::Cmp);

        size_t jmp = emit_jmp(OpCode::Jif);

        // body
        statement();

        // jumps out of switch statement
        jmp_table.push_back(emit_jmp(OpCode::Jump));

        patch_jmp(jmp);
    }

    if(default_label != -1)
        emit_rollback(default_label);

    for(size_t jmp : jmp_table)
        patch_jmp(jmp);

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

    // initializer clause
    if(check(TokenType::Identifier))
    {
        auto identifier = m_current_token.lexeme;

        if(match(TokenType::Identifier) && match(TokenType::In))
        {
            Variable var = build_var(true);

            index = var.index;

            expression();

            set_identifier(var, identifier);

            emit_byte(OpCode::SetMem, var.index);

            consume(TokenType::DotDot, "expected token '..'");

            bool inclusive = match(TokenType::Equal);

            body_start = current_chunk().bytes.size()-2;
            m_loop_starts[m_loop_jmps.size()-1] = body_start;

            // gets index
            emit_byte(OpCode::GetMem, var.index);

            // gets end range
            expression();

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
            var_declaration(false, true, true);
    }
    else
        expression();

#define CONSUME consume(TokenType::SemiColon, "expected ';'");

    CONSUME

    body_start = current_chunk().bytes.size()-2;

    // condition clause

    expression();

    m_loop_starts[m_loop_jmps.size()-1] = current_chunk().bytes.size();

    CONSUME

    exit_jmp = emit_jmp(OpCode::Jif);

    // increment clause
    body_jmp = emit_jmp(OpCode::Jump);
    inc_start = current_chunk().bytes.size()-1;

    expression();

    patch_jmp(body_jmp);

    emit_rollback(body_start);

    body_start = inc_start;

    patch_jmp(body_start);

body:

    // body
    statement();

    if(range_based)
    {
        emit_byte(OpCode::LoadAddr, index);
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

void Compiler::return_stmt()
{
    if(!match(TokenType::SemiColon))
    {
        uint8_t return_count{};

        do
        {
            expression();

            return_count++;

            if(return_count > max_of(return_count))
                return error("you cannot return more than 255 values");

        } while(match(TokenType::Comma));

        if(return_count > 1)
        {
            emit_byte(OpCode::Constant, new Tuple(return_count));
            emit_bytes(OpCode::ConstructTuple);
        }
    }
    else
        emit_bytes(OpCode::Nil);

    emit_bytes(OpCode::Return);
}

void Compiler::declaration()
{
    if(match(TokenType::Var) || match(TokenType::Const))
        var_declaration(true, true, true);
    else if(match(TokenType::Fn))
        fn_declaration(false);
    else
    {
//        if(m_scope_depth == 0)
//            return error("Only declarations are allowed at the global scope");
        statement();
    }

    if(m_panic_mode)
        synchronize();
}

void Compiler::multiple_var_declaration(bool is_const)
{
    uint8_t id_count{};
    uint16_t start_index = m_data_index;

    do
    {
        consume(TokenType::Identifier, "expected identifier");

        std::string_view var_name = m_previous_token.lexeme;

        Variable var
        {
            .depth      = m_scope_depth,
            .is_mutable = !is_const,
            .index      = m_data_index++,
        };

        set_identifier(var, var_name);

        id_count++;

        if(id_count > max_of(id_count))
            return error("you cannot declare more than 255 identifiers in a single declaration");

    } while(match(TokenType::Comma));

    consume(TokenType::RightParen, "expected token ')' after the end of multiple assignment");
    consume(TokenType::Equal, "expected token '='");

    expression();

    emit_byte(OpCode::Constant, start_index);
    emit_byte(OpCode::SetFromTuple, (uint16_t)id_count);
}

void Compiler::var_declaration(bool consume_identifier = true, bool expect_value = true, bool allow_many = true)
{
    bool is_const = m_previous_token.type == TokenType::Const;

    if(match(TokenType::LeftParen))
        return multiple_var_declaration(is_const);

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
    }
    else if(expect_value)
    {
        if(is_const)
            return error("constant variable must be initialized with a value");

        emit_bytes(OpCode::Nil);
    }

    emit_byte(OpCode::SetMem, index);

    set_identifier(var, var_name);

    if(allow_many && match(TokenType::Comma))
    {
        if(!match(TokenType::Identifier))
            return error("expected an identifier after token ','");
        var_declaration(false, expect_value, allow_many);
    }
}

// TODO recursion doesnt work because the function needs to be defined at the top but it needs to set it afterwards
void Compiler::fn_declaration(bool anon_fn)
{
    bool is_named = !anon_fn;

    if(is_named)
        consume(TokenType::Identifier, "expected identifier after fn keyword");

    std::string_view id = is_named ? m_previous_token.lexeme : "fn()";

    uint16_t index = m_data_index++;

    bool is_main = id == "main";

    Function fn = id;

    m_function_stack.push_back(&fn);

    begin_scope();

    consume(TokenType::LeftParen, "expected token '(' after function identifier");

    if(!check(TokenType::RightParen))
    {
        if(is_main)
            return error("main function does not take any arguments");

        do
        {
            var_declaration(true, false, false);

            fn.param_count++;

            if(fn.param_count > max_of(fn.param_count))
                return error("exceeded maximum limit of parameters");

        } while(match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected token matching ')' token");

    if(match(TokenType::Equal))
        expression();
    else
    {
        consume(TokenType::LeftBrace, "expected token '{' or '=' after function signature");

        block();
    }

    emit_bytes(OpCode::Nil, OpCode::Return);

    end_scope();

    m_function_stack.pop_back();

    if(!is_named)
    {
        return emit_byte(OpCode::Constant, new Function(std::move(fn)));
    }

    FunctionData fn_data =
    {
        .param_count = fn.param_count,
        .index = index,
    };

    set_identifier(fn_data, id);

    if(is_main)
        m_entry_fn = std::move(fn);
    else
    {
        emit_byte(OpCode::Constant, new Function(std::move(fn)));
        emit_byte(OpCode::SetMem, index);
    }
}

void Compiler::anon_fn()
{
    fn_declaration(true);
}

int Compiler::resolve_var(std::string_view identifier) const
{
    for(int i = m_scope_depth; i >= 0; i--)
    {
        if(m_identifiers[i].contains(identifier))
            return i;
    }

    return -1;
}

void Compiler::set_identifier(Identifier id, std::string_view var_name)
{
    IDTable &vars = m_identifiers[m_scope_depth];

    if(vars.contains(var_name))
        return error("identifier is already defined in this scope");

    vars.emplace(var_name, id);
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

    (this->*prefix_rule)();

    if(match(TokenType::Eof))
        return;

    while(precedence <= get_rule(m_current_token.type).precedence)
    {
        advance();

        ParseFN infix_rule = get_rule(m_previous_token.type).infix;

        if(infix_rule == nullptr)
            return;

        (this->*infix_rule)();
    }

    if(can_assign && match(TokenType::Equal))
        error("invalid assignment");
}

uint8_t Compiler::_call()
{
    uint8_t arg_count{};

    if(!check(TokenType::RightParen))
    {
        do
        {
            expression();
            arg_count++;

            if(arg_count > max_of(arg_count))
            {
                error("too many args");
                return 0;
            }

        } while(match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected matching ')'");

    return arg_count;
}

inline void Compiler::call()
{
    uint8_t arg_count = _call();
    emit_byte(OpCode::Call, (double)arg_count);
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

inline Chunk& Compiler::current_chunk()
{
    return m_function_stack.empty() ? m_static_chunk : m_function_stack.back()->chunk;
}

inline uint16_t Compiler::id_index(Compiler::Identifier id) const
{
    bool is_var = id.index() == 0;
    return is_var ? std::get<Variable>(id).index : std::get<FunctionData>(id).index;
}

const Compiler::ParseRule Compiler::m_rules[] =
{
        {&Compiler::grouping, &Compiler::call, Precedence::Call}, //leftparen
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
        {&Compiler::identifier,     nullptr,   Precedence::None}, // identifier
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
        {&Compiler::anon_fn,     nullptr,   Precedence::None}, // fn
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







