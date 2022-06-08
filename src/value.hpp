#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include "object.hpp"
#include "util.hpp"

enum class ValueType : uint8_t
{
    Number, Bool, Nil, Object
};

// can reduce struct size by 8 bytes
// NOTE: not supported on call compilers
#pragma pack(push, 1)

struct Value
{
    ValueType type;

    union
    {
        double number;
        bool boolean;
        std::nullptr_t nil;
        Object *object;
    } as{};

    Value() :
        as{.nil = nullptr},
        type(ValueType::Nil)
    {}

    explicit Value(double value) :
        type(ValueType::Number),
        as{.number = value}
    {}

    explicit Value(bool value) :
        type(ValueType::Bool),
        as{.boolean = value}
    {}

    explicit Value(std::nullptr_t value) :
        type(ValueType::Nil),
        as{.nil = value}
    {}

    Value(Object *value) :
        type(ValueType::Object),
        as{.object = value}
    {}

    Value(Value &&value) noexcept
    {
        move_from(std::forward<Value>(value));
    }

    Value(const Value &value)
    {
        copy_from(value);
    }

    ~Value()
    {
        if(type == ValueType::Object)
            delete as.object;
    }

    Value& operator=(Value &&value) noexcept
    {
        move_from(std::forward<Value>(value));
        return *this;
    }

    Value& operator=(const Value &value)
    {
        copy_from(value);
        return *this;
    }

    template<typename T>
    inline T* get() const
    {
        if(type != ValueType::Object)
            return nullptr;
        return static_cast<T*>(as.object);
    }

    inline std::string to_string() const
    {
        using enum ValueType;

        switch(type)
        {
            case Number: return number_str(as.number);
            case Bool:   return as.boolean ? "true" : "false";
            case Nil:    return "nil";
            case Object: return as.object->to_string();
            default:     return "unknown type";
        }
    }

    friend bool operator==(const Value &a, const Value &b)
    {
        if(a.type != b.type)
            return false;

        switch(a.type)
        {
            case ValueType::Nil:    return true;
            case ValueType::Bool:   return a.as.boolean == b.as.boolean;
            case ValueType::Number: return a.as.number  == b.as.number;
            case ValueType::Object: return a.as.object->compare(b.as.object);
            default: return false;
        }
    }

private:

    void move_from(Value &&value)
    {
        type = value.type;

        if(type == ValueType::Object)
        {
            as.object = value.as.object;
            value.as.object = nullptr;
        }
        else
            as = value.as;
    }

    void copy_from(const Value &value)
    {
        type = value.type;

        if(type == ValueType::Object)
            as.object = value.as.object->clone();
        else
            as = value.as;
    }
};

#pragma pack(pop)