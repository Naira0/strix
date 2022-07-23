#pragma once

#include "../types/object.hpp"
#include "../vm.hpp"
#include "../io.hpp"
#include "string.hpp"

struct NativeFunction : Object
{
    typedef InterpretResult(*FN)(VM &vm);

    std::string_view name;
    uint8_t param_count{};

    FN fn;

    NativeFunction() = default;

    NativeFunction(std::string_view name, uint8_t param_count, FN fn) :
        name(name),
        param_count(param_count),
        fn(fn)
    {}

    NativeFunction(const NativeFunction &fn)
    {
        set_fields(fn);
    }

    NativeFunction(NativeFunction &&fn)
    {
        set_fields(fn);
    }

    NativeFunction& operator=(NativeFunction &&fn) noexcept
    {
        set_fields(fn);
        return *this;
    }

    Object* clone() override
    {
        return new NativeFunction(*this);
    }

    Object* move() override
    {
        return new NativeFunction(std::move(*this));
    }

    ObjectType type() const override
    {
        return ObjectType::NativeFunction;
    }

    std::string to_string() const override
    {
        return std::string{name};
    }

private:

    // sets trivial fields for copy and move constructors
    void set_fields(const NativeFunction &f)
    {
        name        = f.name;
        param_count = f.param_count;
        fn          = f.fn;
    }
};

// TODO random numbers, file io, clock, exit, convert print statement to builtins

namespace builtin
{
    static InterpretResult panic /* ! AT THE DISCO */ (VM &vm)
    {
        Value message = vm.pop();

        vm.runtime_error(message.to_string());

        return InterpretResult::RuntimeError;
    }

    static InterpretResult input(VM &vm)
    {
        Value message = vm.pop();

        mio::print(message.to_string());

        std::string buffer;

        std::getline(std::cin, buffer);

        vm.m_stack.emplace_back(new String(std::move(buffer)));

        return InterpretResult::Ok;
    }
}
