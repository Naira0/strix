#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include "object.hpp"
#include "util.hpp"

enum class ValueType : uint8_t
{
    Number, Bool, Nil, Object, Address
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
        uint16_t address; // address for the vms memory
    } as{};

    Value() :
        as{.nil = nullptr},
        type(ValueType::Nil)
    {}

    explicit Value(double value) :
        type(ValueType::Number),
        as{.number = value}
    {}

    explicit Value(uint16_t value) :
        type(ValueType::Address),
        as{.address = value}
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

    std::string to_string() const;


    friend Value operator+(const Value &a, const Value &b);

    friend Value operator-(const Value &a, const Value &b);

    friend Value& operator+=(Value &a, const Value &b);

    friend Value& operator-=(Value &a, const Value &b);

    friend Value& operator*=(Value &a, const Value &b);

    friend Value& operator/=(Value &a, const Value &b);

    friend Value operator/(const Value &a, const Value &b);

    friend Value operator*(const Value &a, const Value &b);

    friend bool operator>(const Value &a, const Value &b);

    friend bool operator<(const Value &a, const Value &b);

    friend bool operator==(const Value &a, const Value &b);

private:
    void move_from(Value &&value);
    void copy_from(const Value &value);

};

#pragma pack(pop)