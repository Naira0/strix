#pragma once

#include <stack>
#include <string_view>
#include <unordered_map>
#include <array>

#include "chunk.hpp"
#include "stack.hpp"

#define DEBUG_TRACE false

constexpr uint16_t MaxDataSize = sizeof(Value) * 1000;
constexpr uint16_t MaxStaticsSize = sizeof(Value) * 2000;

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

    std::array<Value, MaxDataSize> m_variables{};
    std::array<Value, MaxStaticsSize> m_statics{};

    InterpretResult m_state = InterpretResult::Ok;

    InterpretResult run();

    void runtime_error(std::string_view message);

    static bool is_falsy(const Value &value);

    void concat_str();

    bool same_operands(ValueType type) const;
};

