#pragma once

#include <cstdint>
#include <vector>
#include <list>

#include "../value.hpp"

#define FOREACH_OPCODES(e)  \
    e(Constant)             \
    e(SetMem)               \
    e(GetMem)               \
    e(ToString)             \
    e(True)                 \
    e(False)                \
    e(Pop)                  \
    e(Nil)                  \
    e(Cmp)                  \
    e(Greater)              \
    e(Less)                 \
    e(Add)                  \
    e(Subtract)             \
    e(Multiply)             \
    e(Divide)               \
    e(Power)                \
    e(Mod)                  \
    e(Not)                  \
    e(Negate)               \
    e(Increment)            \
    e(Decrement)            \
    e(Or)                   \
    e(And)                  \
    e(Print)                \
    e(LoadAddr)             \
    e(SetCache)             \
    e(LoadCache)            \
    e(TypeCmp)              \
    e(Jif)                  \
    e(Jump)                 \
    e(RollBack)             \
    e(Return)               \
    e(NoOp)                 \


enum class OpCode : uint8_t {FOREACH_OPCODES(GENERATE_ENUM)};

static const char *opcode_str[] = {FOREACH_OPCODES(GENERATE_STRING)};

struct Bytes
{
    OpCode   code;
    uint16_t constant = -1;
    uint32_t line;

    Bytes(OpCode code, size_t constant, uint32_t line) :
        code(code),
        constant(constant),
        line(line)
    {}

    Bytes(OpCode code, uint32_t line) :
            code(code),
            line(line)
    {}
};

struct Chunk
{
    std::vector<Bytes> bytes;
    std::vector<Value> constants;

    inline void set_constant(Value &&value, uint32_t line)
    {
        constants.emplace_back(std::forward<Value>(value));
        bytes.emplace_back(OpCode::Constant,
                           constants.size()-1,
                           line);
    }

    inline void set(OpCode code, Value &&value, uint32_t line)
    {
        constants.emplace_back(std::forward<Value>(value));
        bytes.emplace_back(code, constants.size()-1, line);
    }
};