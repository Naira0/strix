#include <iostream>

#include "chunk.hpp"
#include "debug.hpp"
#include "fmt.hpp"

int simple_instruction(OpCode code, int offset)
{
    fmt::print("{}\n", opcode_str[(uint8_t)code]);
    return offset + 1;
}

int constant_instruction(Value &constant, std::string_view name, int offset)
{
    fmt::print("{} {}\n", name, constant);
    return offset + 1;
}

int disassemble_instruction(Chunk &chunk, Bytes &instruction, int offset)
{
    fmt::print("({}:{}) ", instruction.line, offset);

    using enum OpCode;

    if(instruction.code > Constant)
        return simple_instruction(instruction.code, offset);

    switch(instruction.code)
    {
        case Constant:
            return constant_instruction(chunk.constants[instruction.constant], "Constant", offset);
        default:
            std::cout << "unknown opcode found\n";
            return offset + 1;
    }
}

void disassemble_chunk(Chunk &chunk, std::string_view name)
{
    fmt::print("== {} ==\n", name);

    int offset{};

    while(offset < chunk.bytes.size())
    {
        offset = disassemble_instruction(chunk, chunk.bytes[offset], offset);
    }
}
