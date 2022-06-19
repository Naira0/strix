#pragma once

#include "../types/object.hpp"
#include "../types/chunk.hpp"

struct Function : Object
{
    Chunk chunk;
    std::string_view name;
    std::string_view fn_string;
    uint8_t parem_count;

    ObjectType type() const override
    {
        return ObjectType::Function;
    }

    std::string to_string() override
    {
        return std::string{fn_string};
    }
};