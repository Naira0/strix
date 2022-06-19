#pragma once

#include "../types/object.hpp"

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

    String(std::string &&str);

    ~String() override
    {
        if(!is_static)
            delete[] data;
    }

    String(String &&string) noexcept;

    String(const String &string);

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

    bool compare(const Object *obj) override;


    Object* add(const Object *obj) override;


    Object* plus_equal(const Object *obj) override;


    ObjectType type() const override
    {
        return ObjectType::String;
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
    std::pair<char*, size_t> copy_tobuffer(const String *str) const;
};