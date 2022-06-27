#pragma once

// a scuffed utility to generate enum strings

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

// for converting op code enums to labels for computed gotos
#define GENERATE_GOTO(OPCODE) &&OPCODE,

#include <string>
#include <fstream>

std::string read_file(const char *path);

/*
 * converts a double to a precise string representation removing trailing zeros
 * i tried doing this with sprintf(buff, "%g", value) but i found it performs somewhat the same but it more consistently is slower
 * than my function so i opted to stick with this.
 */
std::string number_str(double value);
