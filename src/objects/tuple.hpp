#pragma once

#include "../types/object.hpp"
#include "../value.hpp"
#include "../util/fmt.hpp"

struct Tuple : Object
{
    Tuple(uint8_t length) :
        length(length)
    {}

    Tuple(const Tuple &tuple) :
        length(tuple.length),
        data(tuple.data)
    {}

    Tuple(Tuple &&tuple) :
        length(tuple.length),
        data(std::move(tuple.data))
    {}

    Object* clone() override
    {
        return new Tuple(*this);
    }

    Object* move() override
    {
        return new Tuple(std::move(*this));
    }

    std::string to_string() const override
    {
        return fmt::format("{}", data);
    }

    ObjectType type() const
    {
        return ObjectType::Tuple;
    }

    std::vector<Value> data;
    uint8_t length;
};