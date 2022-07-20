#pragma once

// a scuffed utility to generate enum strings

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

// for converting op code enums to labels for computed gotos
#define GENERATE_GOTO(OPCODE) &&OPCODE,

#include <string>
#include <optional>
#include <filesystem>
#include <concepts>

std::optional<std::string> read_file(std::filesystem::path &&path);

/*
 * converts a double to a precise string representation removing trailing zeros
 * i tried doing this with sprintf(buff, "%g", value) but i found it performs somewhat the same but it more consistently is slower
 * than my function so i opted to stick with this.
 */
std::string number_str(double value);

template<std::integral T>
inline T max_of(T t)
{
    return std::numeric_limits<T>::max();
}

template<std::integral T>
inline T min_of(T t)
{
    return std::numeric_limits<T>::minx();
}
