
#include <iostream>

#include "vm.hpp"
#include "types/chunk.hpp"
#include "compiler.hpp"
#include "util/fmt.hpp"
#include "util/debug.hpp"
#include "objects/string.hpp"
#include "value.hpp"
#include "objects/tuple.hpp"

#define BINARY_OP(op)               \
    do                              \
    {                               \
          try                       \
          {                         \
              Value b = pop();  \
              Value a = pop();  \
                       \
              m_stack.emplace_back(a op b);  \
          } catch(std::exception &e)\
          {                         \
            runtime_error(e.what());\
          }\
    } while(false)

#define BINARY_OP_MOD(op) \
    do                              \
    {                     \
          uint16_t addr = pop().as.address; \
          Value &mem = m_data[addr];           \
          Value value = pop();              \
          try             \
          {               \
              mem op value;\
          } catch(std::exception &e)\
          {                         \
            runtime_error(e.what());\
          }\
    } while(false)


InterpretResult VM::interpret(std::string_view source)
{
    Compiler compiler(source);

    auto result = compiler.compile();

    if(!result.has_value())
        return InterpretResult::CompileError;

    CallFrame &frame = m_frames[m_frame_cursor];

    frame.function = std::move(result.value());

    return run();
}

InterpretResult VM::run()
{
    using enum OpCode;

#define CHUNK m_frames[m_frame_cursor].function.chunk

#if DEBUG_TRACE
    fmt::print("instructions\n{}\n", CHUNK.bytes);
#endif

    CallFrame *frame  = &m_frames[m_frame_cursor];
    size_t pc = frame->pc;

    while(true)
    {
        if(m_state != InterpretResult::Ok)
            return m_state;

        Bytes instruction = frame->function.chunk.bytes[pc++];

#define CONSTANT frame->function.chunk.constants[instruction.constant]

#if DEBUG_TRACE
        disassemble_instruction(CHUNK, instruction, pc);
#endif
        switch(instruction.code)
        {
            case Constant:
            {
                m_stack.push_back(CONSTANT);
                break;
            }

            case Add:
            {
                if(match(ValueType::Address))
                    BINARY_OP_MOD(+=);
                else
                    BINARY_OP(+); // fix this
                break;
            }
            case Subtract:
            {
                if(match(ValueType::Address))
                    BINARY_OP_MOD(-=);
                else
                    BINARY_OP(-);
                break;
            }
            case Multiply:
            {
                if(match(ValueType::Address))
                    BINARY_OP_MOD(*=);
                else
                    BINARY_OP(*);
                break;
            }
            case Divide:
            {
                if(match(ValueType::Address))
                    BINARY_OP_MOD(/=);
                else
                    BINARY_OP(/);
                break;
            }
            case Greater:  BINARY_OP(>); break;
            case Less:     BINARY_OP(<); break;

            case Mod:
            {
                if(!same_operands(ValueType::Number))
                {
                    runtime_error("operands to binary expression must be numbers");
                    break;
                }

                Value b = pop();
                Value a = pop();

                m_stack.emplace_back(std::fmod(a.as.number, b.as.number));

                break;
            }

            case Power:
            {
                if(!same_operands(ValueType::Number))
                {
                    runtime_error("operands to binary expression must be numbers");
                    break;
                }

                Value b = pop();
                Value a = pop();

                m_stack.emplace_back(std::pow(a.as.number, b.as.number));

                break;
            }

            case True:  m_stack.emplace_back(true);    break;
            case False: m_stack.emplace_back(false);   break;
            case Nil:   m_stack.emplace_back(nullptr); break;

            case Pop:   if(!m_stack.empty()) pop(); break;

            case Cmp:
            {
                Value b = pop();
                Value a = pop();

                m_stack.emplace_back(a == b);

                break;
            }

            case Not: m_stack.emplace_back(is_falsy(pop())); break;

            case Negate:
            {
                if(m_stack.back().type != ValueType::Number)
                {
                    runtime_error("negation operand must be a number");
                    return InterpretResult::RuntimeError;
                }

                m_stack.back().as.number = -m_stack.back().as.number;

                break;
            }

            case Increment:
            {
                Value &value = m_data[pop().as.address];
                value.as.number++;
                break;
            }
            case Decrement:
            {
                Value &value = m_data[pop().as.address];
                value.as.number--;
                break;
            }

            case And:
            {
                Value b = pop();
                Value a = pop();

                m_stack.emplace_back(!is_falsy(a) && !is_falsy(b));

                break;
            }

            case Or:
            {
                Value b = pop();
                Value a = pop();

                // will push the value that is truthy on to the stack
                // this makes expressions like 'var x = nil or "hello"' eval to "hello"
                if(!is_falsy(a))
                    m_stack.emplace_back(a);
                else if(!is_falsy(b))
                    m_stack.emplace_back(b);
                else
                    m_stack.emplace_back(false);

                break;
            }

            case SetMem:
            {
                Value value = pop();
                uint16_t index = CONSTANT.as.address;

                m_data[index] = std::move(value);

                break;
            }
            case GetMem:
            {
                uint16_t index = CONSTANT.as.address;

                Value &value = m_data[index];

                m_stack.push_back(value);

                break;
            }
            case LoadAddr:
            {
                Value &index = CONSTANT;

                m_stack.push_back(index);

                break;
            }
            case TypeCmp:
            {
                Value v1 = pop();
                Value v2 = pop();

                m_stack.emplace_back(v1.type_cmp(v2));

                break;
            }

            case ToString:
            {
                Value value = pop();

                m_stack.emplace_back(new String(value.to_string()));

                break;
            }

            case Jif:
            {
                size_t offset = CONSTANT.as.number;

                if(is_falsy(pop()))
                    pc += offset;

                break;
            }
            case Jump:
            {
                size_t offset = CONSTANT.as.number;

                pc += offset;

                break;
            }
            case RollBack:
            {
                size_t offset = CONSTANT.as.number;

                pc -= offset;

                break;
            }

            case Call:
            {
                auto arg_count = CONSTANT.as.number;



                frame->pc = pc;

                call(arg_count);

                frame = &m_frames[m_frame_cursor];
                pc = frame->pc;

                break;
            }

            case ConstructTuple:
            {
                Value top = pop();

                if(top.type != ValueType::Object || top.as.object->type() != ObjectType::Tuple)
                    return runtime_error("expected tuple");

                auto tuple = top.get<Tuple>();

                if(m_stack.size() < tuple->length)
                    return runtime_error("not enough values on stack for tuple construction");

                for(uint8_t i = 0; i < tuple->length; i++)
                    tuple->data.emplace(tuple->data.begin(), pop());

                m_stack.emplace_back(tuple->move());

                break;
            }

            case SetFromTuple:
            {
                uint16_t id_count = CONSTANT.as.address;

                set_from_tuple(id_count);

                break;
            }

            case NoOp: break;

            case Return:
            {
                if(m_frame_cursor <= 0)
                    return m_state;

                frame->pc = pc;

                frame = &m_frames[--m_frame_cursor];
                pc = frame->pc;
            }
        }
    }

#undef CONSTANT
}

InterpretResult VM::runtime_error(std::string_view message)
{
    CallFrame frame   = m_frames[m_frame_cursor];
    Bytes instruction = frame.function.chunk.bytes[frame.pc];

    fmt::eprint("[runtime error on line {}] {}",
            instruction.line,
            message);

    m_state = InterpretResult::RuntimeError;
    return m_state;
}

inline bool VM::is_falsy(const Value &value)
{
    return
    value.type == ValueType::Nil
    ||
    (value.type == ValueType::Bool && !value.as.boolean);
}

inline bool VM::same_operands(ValueType type) const
{
    auto [a, b] = top_two();
    return a.type == type && b.type == type;
}

inline bool VM::match(ValueType type) const
{
    return m_stack.back().type == type;
}

inline bool VM::is_tuple(Value &value) const
{
    return
    value.type != ValueType::Object ||
    (value.type == ValueType::Object && value.as.object->type() != ObjectType::Tuple);
}

bool VM::same_operands() const
{
    auto [a, b] = top_two();
    return a.type == b.type;
}

inline Value VM::pop()
{
    Value value = std::move(m_stack.back());

    m_stack.pop_back();

    return value;
}

inline std::pair<const Value&, const Value&>
VM::top_two() const
{
    return {m_stack[m_stack.size()-2], m_stack.back()};
}

inline Chunk &VM::chunk()
{
    return m_frames[m_frame_cursor].function.chunk;
}

void VM::call(double arg_count)
{
    Value top = pop();

    if(top.type != ValueType::Object)
    {
        runtime_error("invalid memory called");
        return;
    }

    if(top.as.object->type() == ObjectType::NativeFunction)
    {
        auto native = top.get<NativeFunction>();

        set_fn_params(native->param_count, arg_count);

        m_state = native->fn(*this);

        return;
    }

    auto fn = top.get<Function>();

    if(fn->type() != ObjectType::Function)
    {
        runtime_error("non function called");
        return;
    }

    CallFrame &new_frame = m_frames[++m_frame_cursor];

    new_frame.function = std::move(*fn);
    new_frame.pc = 0;

    set_fn_params(fn->param_count, arg_count);
}

void VM::set_from_tuple(uint16_t id_count)
{
    uint16_t start_index = pop().as.address;

    Value top = pop();

    if(is_tuple(top))
    {
        m_data[start_index] = std::move(top);

        for(uint8_t i = 0; i < id_count; i++)
            m_data[++start_index] = Value(nullptr);

        return;
    }

    auto tuple = top.get<Tuple>();

    for(auto &item : tuple->data)
    {
        m_data[start_index++] = std::move(item);
    }

    if(id_count > tuple->length)
    {
        uint8_t diff = id_count-tuple->length;

        for(uint8_t i = 0; i < diff; i++)
            m_data[start_index++] = Value(nullptr);
    }
}

void VM::set_fn_params(uint8_t param_count, uint8_t arg_count)
{
    uint8_t param_diff = param_count - arg_count;

    if(param_count != arg_count)
    {
        for(uint8_t i = 0; i < param_diff; i++)
            m_stack.emplace_back(nullptr);
    }

    if(param_count <= 1)
        return;

    std::reverse(m_stack.end()-param_count, m_stack.end());
}




