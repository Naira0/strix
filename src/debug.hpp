#pragma once

#include <string_view>

#include "chunk.hpp"

void disassemble_chunk(Chunk &chunk, std::string_view name);
int disassemble_instruction(Chunk &chunk, Bytes &instruction, int offset);