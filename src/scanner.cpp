
#include <cctype>
#include <unordered_map>

#include "scanner.hpp"
#include <iostream>

using enum TokenType;

static const std::unordered_map<std::string_view, TokenType> g_keywords =
{
        {"and",    And},
        {"obj",    Obj},
        {"else",   Else},
        {"false",  False},
        {"true",   True},
        {"for",    For},
        {"fn",     Fn},
        {"if",     If},
        {"do",     Do},
        {"nil",    Nil},
        {"or",     Or},
        {"print",  Print},
        {"return", Return},
        {"super",  Super},
        {"this",   This},
        {"var",    Var},
        {"const",  Const},
        {"while",  While}
};

Token Scanner::scan_token()
{
    skip_chars();

    m_start = m_offset;

    char c = advance();

    switch(c)
    {
        case '\0': return  build(Eof);
        case '(':  return  build(LeftParen);
        case ')':  return  build(RightParen);
        case '{':  return  build(LeftBrace);
        case '}':  return  build(RightBrace);
        case ',':  return  build(Comma);
        case '.':  return  build(Dot);
        case '-':  return  build(Minus);
        case '+':  return  build(Plus);
        case ';':  return  build(SemiColon);
        case '/':  return  build(Slash);
        case '*':  return  build(Star);
        case '%':  return  build(Percent);

        case '!': return build(match('=') ? BangEqual : Bang);
        case '=': return build(match('=') ? EqualEqual : Equal);
        case '>': return build(match('=') ? GreaterEqual: Greater);
        case '<': return build(match('=') ? LessEqual : Less);

        case 'f':
        {
            if(match('"'))
                return build(FStringStart);
            else
                goto build_id;
        }
        case '"': return scan_string();

        default:
            if(isdigit(c))
                return scan_number();
            else if(is_alpha(c))
                build_id: return scan_identifier();
            else
            {
                error("unexpected char");
                build(Error);
            }
    }

    return build(Error);
}

Token Scanner::scan_fstring()
{
    while(!at_end() && peek() != '"')
    {
        m_start = m_offset;

        if(match('}'))
        {
            m_in_fstring_brace = false;
            continue;
        }
        else if(match('{') || m_in_fstring_brace)
        {
            m_in_fstring_brace = true;
            return scan_token();
        }
        else
        {
            while(!at_end() && (peek() != '{' && peek() != '"'))
                advance();

            return build(String);
        }
    }

    if(at_end())
        error("unterminated format string found");

    advance();

    return build(FStringEnd);
}

Token Scanner::scan_string()
{
    while(!at_end() && peek() != '"')
    {
        if(peek() == '\n')
            m_line++;
        advance();
    }

    if(at_end())
        error("unterminated string");

    advance();

    m_start++;
    m_offset--;

    Token token = build(String);

    m_offset++;

    return token;
}

Token Scanner::scan_number()
{
    while(isdigit(peek()))
        advance();

    if(peek() == '.' && isdigit(peek_next()))
    {
        advance();

        while(isdigit(peek()) )
            advance();
    }

    return build(Number);
}

Token Scanner::scan_identifier()
{
    while(is_alpha(peek()) || isdigit(peek()))
        advance();

    std::string_view text = m_source.substr(m_start, m_offset - m_start);
    TokenType type = !g_keywords.contains(text) ? Identifier : g_keywords.at(text);

    return build(type);
}

void Scanner::skip_chars()
{
    char c;

    while((c = peek()))
    {
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t': advance(); break;
            case '\n':
            {
                m_line++;
                m_column = 1;
                m_offset++;
                break;
            }
            case '/':
            {
                if(match_next('/'))
                {
                    while(!at_end() && peek() != '\n')
                        advance();
                    if(peek() == '\n')
                        m_line++;
                }
                else if(match_next('*'))
                {
                    advance();

                    while(!at_end() && peek() != '*' && !match_next('/'))
                    {
                        if(peek() == '\n')
                            m_line++;
                        advance();
                    }

                    advance();

                    if(at_end() || peek() != '/')
                        error("multiline comment is not terminated");
                }
                else
                    return;

                advance();

                break;
            }
            default:
                return;
        }
    }
}

inline Token Scanner::build(TokenType kind) const
{
    return
    {
        .type   = kind,
        .lexeme = m_source.substr(m_start, m_offset - m_start),
        .line   = m_line,
        .column = m_column
    };
}

inline bool Scanner::at_end() const
{
    return m_offset >= m_source.size();
}

inline char Scanner::advance()
{
    if(at_end())
        return '\0';
    m_column++;
    return m_source[m_offset++];
}

inline bool Scanner::match(char c)
{
    if(at_end())
        return false;
    if(m_source[m_offset] != c)
        return false;

    advance();

    return true;
}

bool Scanner::match_next(char c)
{
    if(at_end())
        return false;
    if(m_source[m_offset+1] != c)
        return false;

    advance();

    return true;
}

inline char Scanner::peek() const
{
    if(at_end())
        return '\0';
    return m_source[m_offset];
}

inline char Scanner::peek_next() const
{
    if(m_offset+1 >= m_source.size())
        return '\0';
    return m_source[m_offset+1];
}

bool Scanner::is_alpha(char c) const
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

inline void Scanner::error(std::string_view message)
{
    state.line    = m_line;
    state.column  = m_column;
    state.ok      = false;
    state.message = message;
}