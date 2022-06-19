#include "string.hpp"

std::unordered_map<std::string_view, Object*> String::intern_strings;

String::String(std::string &&str)
{
    char *str_data = &str[0];

    length    = str.size();
    data      = new char[length+1];
    is_static = false;

    std::memcpy(data, str_data, length+1);
}

String::String(String &&string) noexcept
{
    data      = string.data;
    is_static = string.is_static;
    length    = string.length;

    string.data = nullptr;
}

String::String(const String &string)
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

std::pair<char *, size_t> String::copy_tobuffer(const String *str) const
{
    size_t new_len = length + str->length;

    auto buffer = new char[new_len + 2];

    std::memcpy(buffer, data, length);
    std::memcpy(buffer + length, str->data, str->length);

    buffer[new_len] = '\0';

    return {buffer, new_len};
}

bool String::compare(const Object *obj)
{
    if(obj->type() != ObjectType::String)
        return false;

    auto str = static_cast<const String*>(obj);

    // compares object pointers to do constant time string comparisons
    auto s1 = intern_strings.find(str->to_sv());
    auto s2 = intern_strings.find(to_sv());

    return s1 == s2;
}

Object *String::add(const Object *obj)
{
    auto str = static_cast<const String*>(obj);

    auto [buffer, new_len] = copy_tobuffer(str);

    return new String(buffer, new_len);
}

Object *String::plus_equal(const Object *obj)
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