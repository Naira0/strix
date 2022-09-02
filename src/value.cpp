#include "value.hpp"
#include <exception>
#include <complex>

void Value::move_from(Value &&value)
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

void Value::copy_from(const Value &value)
{
    type = value.type;

    if(type == ValueType::Object)
        as.object = value.as.object->clone();
    else
        as = value.as;
}

bool operator==(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return true;
        case ValueType::Bool:   return a.as.boolean == b.as.boolean;
        case ValueType::Number: return a.as.number  == b.as.number;
        case ValueType::Object: return a.as.object->compare(b.as.object);
        default: return false;
    }
}

Value operator+(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return nullptr;
        case ValueType::Bool:   return Value((double)a.as.boolean + b.as.boolean);
        case ValueType::Number: return Value(a.as.number + b.as.number);
        case ValueType::Object: return a.as.object->add(b.as.object);
        default: return nullptr;
    }
}

std::string Value::to_string() const
{
    using enum ValueType;

    switch(type)
    {
        case Number:  return number_str(as.number);
        case Bool:    return as.boolean ? "true" : "false";
        case Nil:     return "nil";
        case Object:  return as.object->to_string();
        case Address: return std::to_string(as.address);
        default:      return "unknown type";
    }
}

Value operator-(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return nullptr;
        case ValueType::Bool:   return Value((double)a.as.boolean - b.as.boolean);
        case ValueType::Number: return Value(a.as.number - b.as.number);
        case ValueType::Object: return a.as.object->subtract(b.as.object);
        default: return nullptr;
    }
}

Value &operator+=(Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: a.as.number += b.as.number; break;
        case ValueType::Object:
            a.as.object->plus_equal(b.as.object); break;
    }

    return a;
}

Value operator/(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: return Value(a.as.number / b.as.number);
        default: return nullptr;
    }
}

Value operator*(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: return Value(a.as.number * b.as.number);
        default: return nullptr;
    }
}

bool operator>(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return false;
        case ValueType::Bool:   return a.as.boolean > b.as.boolean;
        case ValueType::Number: return a.as.number > b.as.number;
        default: return false;
    }
}

bool operator<(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return false;
        case ValueType::Bool:   return a.as.boolean < b.as.boolean;
        case ValueType::Number: return a.as.number < b.as.number;
        default: return false;
    }
}

Value &operator-=(Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: a.as.number -= b.as.number; break;
        case ValueType::Object: a.as.object->minus_equal(b.as.object); break;
    }

    return a;
}

Value &operator*=(Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: a.as.number *= b.as.number; break;
        case ValueType::Object: a.as.object->multiply_equal(b.as.object); break;
    }

    return a;
}

Value &operator/=(Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Number: a.as.number /= b.as.number; break;
        case ValueType::Object: a.as.object->divide_equal(b.as.object); break;
    }

    return a;
}

bool operator<=(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return false;
        case ValueType::Number: return a.as.number <= b.as.number;
        default: return false;
    }
}

bool operator>=(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return false;
        case ValueType::Number: return a.as.number >= b.as.number;
        default: return false;
    }
}

Value Value::power(const Value &b) const
{
    if(type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(type)
    {
        case ValueType::Number: return Value(std::pow(as.number, b.as.number));
        default: return nullptr;
    }
}

Value Value::mod(const Value &b) const
{
    if(type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(type)
    {
        case ValueType::Number: return Value(std::fmod(as.number, b.as.number));
        default: return nullptr;
    }
}

bool Value::is_falsy() const
{
    return
        type == ValueType::Nil
        ||
        (type == ValueType::Bool && !as.boolean);
}

bool Value::type_cmp(const Value &b) const
{
    return type == b.type;
}

bool operator!=(const Value &a, const Value &b)
{
    if(a.type != b.type)
        throw std::runtime_error("invalid operands to binary expression");

    switch(a.type)
    {
        case ValueType::Nil:    return true;
        case ValueType::Bool:   return a.as.boolean != b.as.boolean;
        case ValueType::Number: return a.as.number  != b.as.number;
        case ValueType::Object: return !a.as.object->compare(b.as.object);
        default: return false;
    }
}




