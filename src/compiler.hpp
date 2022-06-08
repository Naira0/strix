#pragma once

#include <string_view>
#include <array>
#include <variant>

#include "chunk.hpp"
#include "scanner.hpp"
#include "token.hpp"

#define DEBUG_TOKENS false

enum class Precedence : uint8_t
{
    None,
    Assignment,
    Or,
    And,
    Equality,
    Comparison,
    Term,
    Factor,
    Unary,
    Call,
    Primary
};

enum class ParseState : uint8_t
{
    None,
    FString,
};

class Compiler
{
public:
    Compiler(std::string_view source, Chunk &chunk)
    :
        m_scanner(source),
        m_current_chunk(chunk)
    {}

    bool compile();

private:
    Scanner m_scanner;

    Token m_previous_token;
    Token m_current_token;

    Chunk &m_current_chunk;

    bool m_had_error = false;
    bool m_panic_mode = false;
    bool m_can_assign;

    ParseState m_state = ParseState::None;

    struct Variable
    {
        size_t depth;
        bool is_mutable;
        int  index;
    };

    using VarTable = std::unordered_map<std::string_view, Variable>;

    // each index represents the current scope with 0 being global
    std::vector<VarTable> m_variables { VarTable() };
    size_t m_scope_depth = 0;

    // counters for variable index that mirrors the vms arrays
    int m_var_index = 0;
    int m_static_index = 0;

    typedef void(Compiler::*ParseFN)();

    struct ParseRule
    {
        ParseFN prefix;
        ParseFN infix;
        Precedence precedence;
    };

    static const ParseRule m_rules[];

    void advance();

    void expression();

    void consume(TokenType type, std::string_view message);

    void error_at(Token &token, std::string_view message);
    void error(std::string_view message);

    ParseRule get_rule(TokenType kind) const;

    template<typename ...A>
    void emit_bytes(A ...a);

    size_t emit_jmp(OpCode instruction);

    void patch_jmp(size_t offset);

    void emit_rollback(size_t start);

    void number();

    void string();

    void fstring();

    void grouping();

    void binary();

    void unary();

    void literal();

    void variable();

    void print_stmt();

    void begin_scope();

    void end_scope();

    void block();

    void statement();

    void if_stmt();

    void if_expr();

    void while_stmt();

    void declaration();

    void var_declaration();

    int resolve_scope();

    void parse_variable(Variable var);

    void synchronize();

    void emit_constant(Value &&value);

    void parse_precedence(Precedence precedence);

    // the syntax for calling member function pointers is ugly enough where this method is justified
    // ie (this->*fn)(); doesnt seem so bad until you have it all over your code
    void call(Compiler::ParseFN fn);

    bool check(TokenType type) const;

    bool match(TokenType type);

};
