#pragma once

#include "../types/object.hpp"
#include "../types/chunk.hpp"

struct Function : Object
{
    Chunk chunk;
    std::string_view name;
    std::string_view fn_string;
    uint8_t param_count{};

    Function() = default;

    Function(std::string_view name) :
        name(name)
    {}

    Function(const Function &fn) :
        chunk(fn.chunk)
    {
        set_fields(fn);
    }

    Function(Function &&fn):
        chunk(std::move(fn.chunk))
    {
        set_fields(fn);
    }

    Function& operator=(Function &&fn) noexcept
    {
        set_fields(fn);
        chunk = std::move(fn.chunk);
        return *this;
    }

    Object* clone() override
    {
        return new Function(*this);
    }

    Object* move() override
    {
        return new Function(std::move(*this));
    }

    ObjectType type() const override
    {
        return ObjectType::Function;
    }

    std::string to_string() const override
    {
        return std::string{name};
    }

private:

    // sets trivial fields for copy and move constructors
    void set_fields(const Function &fn)
    {
        name        = fn.name;
        fn_string   = fn.fn_string;
        param_count = fn.param_count;
    }
};

