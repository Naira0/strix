
#include <iostream>

#include "vm.hpp"
#include "chunk.hpp"
#include "compiler.hpp"
#include "fmt.hpp"
#include "debug.hpp"

#define BINARY_OP(op)               \
    do                              \
    {                               \
          if(!same_operands(ValueType::Number)) \
          {                   \
                runtime_error("operands to binary expression must be numbers"); \
                break;\
          }\
          Value b = m_stack.pop();  \
          Value a = m_stack.pop();  \
                   \
          m_stack.emplace(a.as.number op b.as.number);  \
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

            case Add:
            {
                if(same_operands(ValueType::Object))
                    concat_str();
                else if(same_operands(ValueType::Number))
                    BINARY_OP(+);
                else
                    runtime_error("invalid operands provided to binary expression");
                break;
            }
            case Subtract: BINARY_OP(-); break;
            case Multiply: BINARY_OP(*); break;
            case Divide:   BINARY_OP(/); break;
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

            case SetVar:
            {
                Value value = m_stack.pop();
                int index = m_stack.pop().as.number;

                m_variables[index] = std::move(value);

                break;
            }
            case GetVar:
            {
                int index = m_stack.pop().as.number;

                Value &value = m_variables[index];

                m_stack.push(value);

                break;
            }
            case SetStatic:
            {
                Value value = m_stack.pop();
                int index = m_stack.pop().as.number;

                m_statics[index] = std::move(value);

                break;
            }
            case GetStatic:
            {
                int index = m_stack.pop().as.number;

                Value &value = m_statics[index];

                m_stack.push(value);

                break;
            }

            case ToString:
            {
                Value value = m_stack.pop();

                m_stack.push(new String(value.to_string()));

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

            case Return:
            {
                //fmt::print("{} items on the stack", m_stack.size());
                return InterpretResult::Ok;
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

void VM::concat_str()
{
    Value b = m_stack.pop();
    Value a = m_stack.pop();

    auto s2 = b.get<String>();
    auto s1 = a.get<String>();

    if(!s1 || !s2)
        runtime_error("invalid strings on stack");

    size_t length = s1->length + s2->length;

    char *data = new char[length+2];

    std::memcpy(data, s1->data, s1->length);
    std::memcpy(data+s1->length, s2->data, s2->length);

    data[length] = '\0';

    m_stack.emplace(new String(data, length));
}

inline bool VM::same_operands(ValueType type) const
{
    auto [a, b] = m_stack.top_two();
    return a.type == type && b.type == type;
}


