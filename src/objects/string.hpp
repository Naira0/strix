#pragma once

#include "../types/object.hpp"

struct String : Object
{
    // this is fairly safe as it will never attempt to mutate the data if it is set as static
    String(std::string_view sv) :
          data(sv)
    {
        intern_strings.emplace(sv, this);
    }

    String(std::string &&string) :
        data(std::forward<std::string>(string))
    {
        intern_strings.emplace(data, this);
    }

    String(String &&string) noexcept :
        data(std::move(string.data))
    {}

    String(const String &string) :
        data(string.data)
    {}

    Object* clone() override
    {
        return new String(*this);
    }

    Object* move() override
    {
        return new String(std::move(*this));
    }

    std::string to_string() const override
    {
        return data;
    }

    bool compare(const Object *obj) override;

    Object* add(const Object *obj) override;

    Object* plus_equal(const Object *obj) override;

    ObjectType type() const override
    {
        return ObjectType::String;
    }

    std::string data;

    // this static map is used for string interning
    static std::unordered_map<std::string_view, Object*> intern_strings;
};