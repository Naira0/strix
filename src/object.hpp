#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <iostream>
#include <unordered_map>

#include "util.hpp"

#define FOREACH_OBJTYPE(e) \
         e(String)         \


enum class ObjectType : uint8_t
{ FOREACH_OBJTYPE(GENERATE_ENUM) };

static const char *obj_type_str[] =
{ FOREACH_OBJTYPE(GENERATE_STRING) };

struct Object
{
    virtual ~Object() = default;

    virtual Object* clone() = 0;
    virtual Object* move() = 0;

    virtual std::string to_string() = 0;

    virtual ObjectType type() const = 0;

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

struct String : Object
{

    // this is fairly safe as it will never attempt to mutate the data if it is set as static
    String(std::string_view sv) :
        is_static(true),
        data(const_cast<char*>(sv.data())),
        length(sv.size())
    {
        intern_strings.emplace(sv, this);
    }

    String(char *data, size_t length) :
        is_static(false),
        data(data),
        length(length)
    {
        intern_strings.emplace(data, this);
    }

    String(std::string &&str)
    {
        char *str_data = &str[0];

        length    = str.size();
        data      = new char[length+1];
        is_static = false;

        std::memcpy(data, str_data, length+1);
    }

    ~String() override
    {
        if(!is_static)
            delete[] data;
    }

    String(String &&string) noexcept
    {
        data      = string.data;
        is_static = string.is_static;
        length    = string.length;

        string.data = nullptr;
    }

    String(const String &string)
    {
        is_static = string.is_static;
        length    = string.length;

        if(is_static)
        {
            data = string.data;
        }
        else
        {
            data = new char[length+1];
            std::strcpy(data, string.data);
        }
    }

    Object* clone() override
    {
        return new String(*this);
    }

    Object* move() override
    {
        return new String(std::move(*this));
    }

    std::string to_string() override
    {
        if(!is_static)
            return data;
        return std::string{to_sv()};
    }

    bool compare(const Object *obj) override
    {
        if(obj->type() != ObjectType::String)
            return false;

        auto str = static_cast<const String*>(obj);

        // compares object pointers to do constant time string comparisons
        auto s1 = intern_strings.find(str->to_sv());
        auto s2 = intern_strings.find(to_sv());

        return s1 == s2;
    }

    Object* add(const Object *obj) override
    {
        auto str = static_cast<const String*>(obj);

        auto [buffer, new_len] = copy_tobuffer(str);

        return new String(buffer, new_len);
    }

    Object* plus_equal(const Object *obj) override
    {
        auto str = static_cast<const String*>(obj);

        auto [buffer, new_len] = copy_tobuffer(str);

        if(!is_static)
            delete[] data;

        data      = buffer;
        length    = new_len;
        is_static = false;

        return this;
    }

    ObjectType type() const override
    {
        return m_type;
    }

    inline std::string_view to_sv() const
    {
        return {data, length};
    }

    char *data;
    bool is_static;
    size_t length;

    // this static map is used for string interning
    static std::unordered_map<std::string_view, Object*> intern_strings;

private:

    ObjectType m_type = ObjectType::String;

    std::pair<char*, size_t> copy_tobuffer(const String *str) const
    {
        size_t new_len = length + str->length;

        auto buffer = new char[new_len + 2];

        std::memcpy(buffer, data, length);
        std::memcpy(buffer + length, str->data, str->length);

        buffer[new_len] = '\0';

        return {buffer, new_len};
    }
};

