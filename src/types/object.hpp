#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <iostream>
#include <unordered_map>

#include "../util/util.hpp"

#define FOREACH_OBJTYPE(e)  \
         e(String)          \
         e(Function)        \
         e(Tuple)           \


enum class ObjectType : uint8_t
{ FOREACH_OBJTYPE(GENERATE_ENUM) };

static const char *obj_type_str[] =
{ FOREACH_OBJTYPE(GENERATE_STRING) };

// TODO add subscript support
struct Object
{
    virtual ~Object() = default;

    virtual Object* clone() = 0;
    virtual Object* move() = 0;

    virtual std::string to_string() const = 0;

    virtual ObjectType type() const = 0;

    bool is(ObjectType obj_type) const
    {
        return type() == obj_type;
    }

    virtual bool compare(const Object *obj) { return false; }

    virtual Object* add(const Object *obj) { return nullptr; }

    virtual Object* subtract(const Object *obj) { return nullptr; }

    virtual Object* divide(const Object *obj) { return nullptr; }

    virtual Object* multiply(const Object *obj) { return nullptr; }

    virtual Object* plus_equal(const Object *obj) { return nullptr; }

    virtual Object* minus_equal(const Object *obj) { return nullptr; }

    virtual Object* divide_equal(const Object *obj) { return nullptr; }

    virtual Object* multiply_equal(const Object *obj) { return nullptr; }

private:
    ObjectType m_type;
};