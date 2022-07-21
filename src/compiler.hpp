#pragma once

#include <string_view>
#include <array>
#include <variant>

#include "types/chunk.hpp"
#include "scanner.hpp"
#include "types/token.hpp"
#include "objects/function.hpp"

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
    Compiler(std::string_view source)
    :
            m_scanner(source)
    {}

    std::optional<Function> compile();

private:

    Scanner m_scanner;

    Token m_previous_token;
    Token m_current_token;

    // the top level chunk for statics (globals)
    Chunk m_static_chunk;

    bool m_had_error = false;
    bool m_panic_mode = false;
    bool m_can_assign;

    // used for program entry (main function) will be called at the end of the static chunk
    Function m_entry_fn;

    // used to determine which chunk the bytes will be written into
    std::vector<Function*> m_function_stack;

    ParseState m_state = ParseState::None;

    struct Variable
    {
        size_t depth;
        bool is_mutable;
        uint16_t index;
    };

    struct FunctionData
    {
        uint8_t parem_count;
        uint16_t index;
    };

    using Identifier = std::variant<Variable, FunctionData>;

    using IDTable = std::unordered_map<std::string_view, Identifier>;

    // each index represents the current scope with 0 being global
    std::vector<IDTable> m_identifiers { IDTable() };
    size_t m_scope_depth = 0;

    // counter for data index that mirrors the vms arrays
    uint16_t m_data_index = 0;

    // elements are the start of the loop at the current loop depth
    std::array<size_t, 50> m_loop_starts;
    // index in first dimension is the loop depth
    // in the second its the index of the byte that must be updated in the chunk
    std::vector<std::vector<size_t>> m_loop_jmps;

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

    template<typename T>
    void emit_byte(OpCode code, T &&value);

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

    void identifier();

    OpCode mod_assignable(Variable var, bool &get_mem);

    void print_stmt();

    void begin_scope();

    void end_scope();

    void block();

    void statement();

    void continue_break_stmt();

    void if_stmt();

    void if_expr();

    void while_stmt();

    void switch_stmt();

    void for_stmt();

    void return_stmt();

    void declaration();

    void multiple_var_declaration(bool is_const);

    void var_declaration(bool consume_identifier, bool expect_value, bool allow_many);

    void fn_declaration(bool anon_fn = false);

    void anon_fn();

    int resolve_var(std::string_view identifier) const;

    void set_identifier(Identifier id, std::string_view var_name);

    void synchronize();

    void parse_precedence(Precedence precedence);

    // the syntax for calling member function pointers is ugly enough where this method is justified
    // ie (this->*fn)(); doesnt seem so bad until you have it all over your code
    void call();

    uint8_t _call();

    bool check(TokenType type) const;

    bool check_last(TokenType type) const;

    bool match(TokenType type);

    Compiler::Variable build_var(bool is_mutable);

    Chunk& current_chunk();

    uint16_t id_index(Identifier id) const;
};
