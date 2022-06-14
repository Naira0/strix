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


struct Cache
{
    enum class Type : uint8_t { Var, Value };

    struct Item
    {
        Type type;
        std::string_view lexeme;
        Value value;
    };

    std::vector<Item> items;

    void set(Value &&value)
    {
        items.emplace_back(Type::Value, "", std::forward<Value>(value));
    }

    Item get()
    {
        Item value = std::move(items.back());
        items.pop_back();
        return value;
    }
};


class Compiler
{
public:
    Compiler(std::string_view source, Chunk &chunk)
    :
            m_scanner(source),
            m_chunk(chunk)
    {}

    bool compile();

private:
    Scanner m_scanner;

    Token m_previous_token;
    Token m_current_token;

    Chunk &m_chunk;

    bool m_had_error = false;
    bool m_panic_mode = false;
    bool m_can_assign;

    ParseState m_state = ParseState::None;

    struct Variable
    {
        size_t depth;
        bool is_mutable;
        uint16_t index;
        bool value_known;
        Value *value;
        ValueType type;
    };

    using VarTable = std::unordered_map<std::string_view, Variable>;

    // each index represents the current scope with 0 being global
    std::vector<VarTable> m_variables { VarTable() };
    size_t m_scope_depth = 0;

    // counter for data index that mirrors the vms arrays
    int m_data_index = 0;

    Cache m_cache;

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

    bool emit_cache();

    bool is_known(const Cache::Item &item) const;

    bool is_known() const;

    bool is_two_known() const;

    void number();

    void string();

    void fstring();

    void grouping();

    void binary();

    void unary();

    void literal();

    void variable();

    OpCode mod_assignable(Variable var, bool &get_mem);

    void print_stmt();

    void begin_scope();

    void end_scope();

    void block();

    void statement();

    void if_stmt();

    void if_expr();

    void while_stmt();

    void for_stmt();

    void declaration();

    void var_declaration();

    int resolve_var(std::string_view identifier) const;

    void set_var(Variable var, std::string_view var_name);

    void synchronize();

    void emit_constant(Value &&value);

    void parse_precedence(Precedence precedence);

    // the syntax for calling member function pointers is ugly enough where this method is justified
    // ie (this->*fn)(); doesnt seem so bad until you have it all over your code
    void call(Compiler::ParseFN fn);

    bool check(TokenType type) const;

    bool match(TokenType type);

};
