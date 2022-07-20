#pragma once

#include <cstdint>
#include <string_view>

#include "types/token.hpp"

class Scanner
{
public:

    Scanner(std::string_view source)
    :
        m_source(source)
    {}

    Token scan_token();

    Token scan_fstring();

    struct State
    {
        uint32_t line;
        uint32_t  column;
        bool ok = true;
        std::string_view message;
    } state{};

private:
    size_t   m_offset = 0;
    size_t   m_start  = 0;
    size_t   m_line   = 1;
    size_t   m_column = 1;

    std::string_view m_source;

    Token m_last;

    bool m_in_fstring_brace = false;

    Token build(TokenType kind);

    // skips non-token chars
    bool skip_chars();

    Token scan_string();
    Token scan_number();
    Token scan_identifier();

    bool at_end() const;
    char advance();
    bool match(char c);
    bool match_next(char c);
    char peek() const;
    char peek_next() const;
    bool is_alpha(char c) const;

    void error(std::string_view message);
};