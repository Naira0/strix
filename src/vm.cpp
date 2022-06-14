
#include <iostream>

#include "vm.hpp"
#include "chunk.hpp"
#include "compiler.hpp"
#include "fmt.hpp"
#include "debug.hpp"

#define BINARY_OP(op)               \
    do                              \
    {                               \
          if(!same_operands()) \
          {                   \
                runtime_error("invalid operands provided to binary expression"); \
                break;\
          }                         \
          try                       \
          {                         \
              Value b = m_stack.pop();  \
              Value a = m_stack.pop();  \
                       \
              m_stack.emplace(a op b);  \
          } catch(std::exception &e)\
          {                         \
            runtime_error(e.what());\
          }\
    } while(false)

#define BINARY_OP_MOD(op) \
    do                              \
    {                     \
          uint16_t addr = m_stack.pop().as.address; \
          Value &mem = m_data[addr];           \
          Value value = m_stack.pop();              \
                          \
          mem op value;\
    } while(false)


InterpretResult VM::interpret(std::string_view source)
{
    Compiler compiler(source, m_chunk);

    bool ok = compiler.compile();

    if(!ok)
        return InterpretResult::CompileError;

    return run();
}

InterpretResult VM::run()
{
    using enum OpCode;

#if DEBUG_TRACE
    fmt::print("instructions\n{}\n", m_chunk.bytes);
#endif

    while(true)
    {
        if(m_state != InterpretResult::Ok)
            return m_state;

        Bytes instruction = m_chunk.bytes[m_pc++];

#if DEBUG_TRACE
        disassemble_instruction(m_chunk, instruction, m_pc);
#endif

        switch(instruction.code)
        {
            case Constant:
            {
                m_stack.push(m_chunk.constants[instruction.constant]);
                break;
            }

            // TODO: implement the rest of the op= operators
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

                Value b = m_stack.pop();
                Value a = m_stack.pop();

                m_stack.emplace(std::fmod(a.as.number, b.as.number));

                break;
            }

            case Power:
            {
                if(!same_operands(ValueType::Number))
                {
                    runtime_error("operands to binary expression must be numbers");
                    break;
                }

                Value b = m_stack.pop();
                Value a = m_stack.pop();

                m_stack.emplace(std::pow(a.as.number, b.as.number));

                break;
            }

            case True:  m_stack.emplace(true);    break;
            case False: m_stack.emplace(false);   break;
            case Nil:   m_stack.emplace(nullptr); break;

            //case Pop: if(!m_stack.empty()) m_stack.pop(); break;

            case Equal:
            {
                Value b = m_stack.pop();
                Value a = m_stack.pop();

                m_stack.emplace(a == b);

                break;
            }

            case Not: m_stack.emplace(is_falsy(m_stack.pop())); break;

            case Negate:
            {
                if(m_stack.top().type != ValueType::Number)
                {
                    runtime_error("negation operand must be a number");
                    return InterpretResult::RuntimeError;
                }

                m_stack.top().as.number = -m_stack.top().as.number;

                break;
            }

            case Increment:
            {
                Value &value = m_data[m_stack.pop().as.address];
                value.as.number++;
                break;
            }
            // TODO: implement in compiler
            case Decrement:
            {
                Value &value = m_data[m_stack.pop().as.address];
                value.as.number--;
                break;
            }

            case And:
            {
                Value b = m_stack.pop();
                Value a = m_stack.pop();

                m_stack.emplace(!is_falsy(a) && !is_falsy(b));

                break;
            }

            case Or:
            {
                Value b = m_stack.pop();
                Value a = m_stack.pop();

                m_stack.emplace(!is_falsy(a) || !is_falsy(b));

                break;
            }

            case SetMem:
            {
                Value value = m_stack.pop();
                uint16_t index = m_chunk.constants[instruction.constant].as.address;

                m_data[index] = std::move(value);

                break;
            }
            case GetMem:
            {
                uint16_t index = m_chunk.constants[instruction.constant].as.address;

                Value &value = m_data[index];

                m_stack.push(value);

                break;
            }
            case LoadAddr:
            {
                Value &index = m_chunk.constants[instruction.constant];

                m_stack.push(index);

                break;
            }

            case TypeCmp:
            {
                Value v1 = m_stack.pop();
                Value v2 = m_stack.pop();

                m_stack.emplace(v1.type_cmp(v2));

                break;
            }

            case ToString:
            {
                Value value = m_stack.pop();

                m_stack.emplace(new String(value.to_string()));

                break;
            }

            case Jif:
            {
                size_t offset = m_chunk.constants[instruction.constant].as.number;

                if(is_falsy(m_stack.pop()))
                    m_pc += offset;

                break;
            }
            case Jump:
            {
                size_t offset = m_chunk.constants[instruction.constant].as.number;

                m_pc += offset;

                break;
            }
            case RollBack:
            {
                size_t offset = m_chunk.constants[instruction.constant].as.number;

                m_pc -= offset;

                break;
            }

            case Print: fmt::print("{}\n", m_stack.pop()); break;

            case NoOp: break;

            case Return:
            {
                return m_state;
            }
        }
    }
}

void VM::runtime_error(std::string_view message)
{
    Bytes instruction = m_chunk.bytes[m_pc];

    fmt::eprint("[runtime error on line {}] {}",
            instruction.line,
            message);

    m_state = InterpretResult::RuntimeError;
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
    auto [a, b] = m_stack.top_two();
    return a.type == type && b.type == type;
}

bool VM::match_last(ValueType type) const
{
    auto last = m_stack.tail_node()->last;

    if(last != nullptr && last->data.type == type)
        return true;

    return false;
}

bool VM::match(ValueType type) const
{
    return m_stack.top().type == type;
}

bool VM::same_operands() const
{
    auto [a, b] = m_stack.top_two();
    return a.type == b.type;
}

Value &VM::from_addr(Bytes instruction)
{
    uint16_t addr = m_chunk.constants[instruction.constant].as.address;
    return m_data[addr];
}


