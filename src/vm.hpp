#pragma once

#include <stack>
#include <string_view>
#include <unordered_map>
#include <array>

#include "types/chunk.hpp"
#include "objects/function.hpp"

#define DEBUG_TRACE false

constexpr uint16_t MaxDataSize = sizeof(Value) * 1000;
constexpr uint8_t MaxCallFrames = 255;

enum class InterpretResult
{
    Ok,
    CompileError,
    RuntimeError
};

struct CallFrame
{
    Function  function{};
    size_t    pc{};
};

class VM
{
public:

    InterpretResult interpret(std::string_view source);

    VM()
    {
        m_stack.reserve(1000);
    }

private:

    std::array<CallFrame, MaxCallFrames> m_frames{};
    uint8_t m_frame_cursor{};

    std::vector<Value> m_stack;

    std::array<Value, MaxDataSize> m_data; // the vms internal memory used for various things (caching, variables, functions)

    InterpretResult m_state = InterpretResult::Ok;

    InterpretResult run();

    InterpretResult runtime_error(std::string_view message);

    static bool is_falsy(const Value &value);

    bool same_operands(ValueType type) const;

    bool same_operands() const;

    bool match(ValueType type) const;

    Value pop();

    std::pair<const Value&, const Value&> top_two() const;

    Chunk &chunk();

    void call(double arg_count);

    void set_from_tuple(uint16_t id_count);

};

