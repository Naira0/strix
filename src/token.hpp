#pragma once

#include <cstdint>
#include <string_view>
#include <ostream>

#include "util.hpp"

#define FOREACH_TOKENTYPE(e) \
    e(LeftParen)             \
    e(RightParen)            \
    e(LeftBrace)             \
    e(RightBrace)            \
    e(Comma)                 \
    e(Dot)                   \
    e(Minus)                 \
    e(Plus)                  \
    e(SemiColon)             \
    e(Slash)                 \
    e(Star)                  \
    e(PlusEqual)             \
    e(MinusEqual)            \
    e(StarEqual)             \
    e(SlashEqual)            \
    e(PlusPlus)              \
    e(MinusMinus)            \
    e(Caret)                 \
    e(Percent)               \
    e(Bang)                  \
    e(BangEqual)             \
    e(Equal)                 \
    e(EqualEqual)            \
    e(Greater)               \
    e(GreaterEqual)          \
    e(Less)                  \
    e(LessEqual)             \
    e(Identifier)            \
    e(String)                \
    e(FStringStart)          \
    e(FStringEnd)            \
    e(Number)                \
    e(And)                   \
    e(Is)                    \
    e(Else)                  \
    e(False)                 \
    e(True)                  \
    e(Nil)                   \
    e(Do)                    \
    e(Or)                    \
    e(Obj)                   \
    e(For)                   \
    e(If)                    \
    e(Fn)                    \
    e(Print)                 \
    e(Return)                \
    e(Super)                 \
    e(This)                  \
    e(Var)                   \
    e(Const)                 \
    e(While)                 \
    e(Error)                 \
    e(Eof)                   \

enum class TokenType : uint8_t
{
   FOREACH_TOKENTYPE(GENERATE_ENUM)
};

static const char *tk_type_str[] =
{
   FOREACH_TOKENTYPE(GENERATE_STRING)
};

struct Token
{
    TokenType type;
    std::string_view lexeme;
    uint32_t line;
    uint32_t column;
};