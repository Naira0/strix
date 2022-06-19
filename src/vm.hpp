#pragma once

#include <stack>
#include <string_view>
#include <unordered_map>
#include <array>

#include "types/chunk.hpp"
#include "data-structures/stack.hpp"

#define DEBUG_TRACE true

constexpr uint16_t MaxDataSize = sizeof(Value) * 1000;

enum class InterpretResult
{
    Ok,
    CompileError,
    RuntimeError
};

class VM
{
public:

    InterpretResult interpret(std::string_view source);

private:

    Chunk m_chunk;
    size_t m_pc{};

    Stack<Value> m_stack;

    std::array<Value, MaxDataSize> m_data;

    InterpretResult m_state = InterpretResult::Ok;

    InterpretResult run();

    void runtime_error(std::string_view message);

    static bool is_falsy(const Value &value);

    bool same_operands(ValueType type) const;

    bool same_operands() const;

    bool match_last(ValueType type) const;

    bool match(ValueType type) const;

    Value& from_addr(Bytes instruction);
};

