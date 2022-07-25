#include "string.hpp"

std::unordered_map<std::string_view, Object*> String::intern_strings;

bool String::compare(const Object *obj)
{
    if(obj->type() != ObjectType::String)
        return false;

    auto str = static_cast<const String*>(obj);

    // compares object pointers to do constant time string comparisons
    auto s1 = intern_strings.find(str->data);
    auto s2 = intern_strings.find(data);

    return s1 == s2;
}

Object *String::add(const Object *obj)
{
    auto str = static_cast<const String*>(obj);
    return new String(data + str->data);
}

Object *String::plus_equal(const Object *obj)
{
    auto str = static_cast<const String*>(obj);

    data += str->data;

    return this;
}