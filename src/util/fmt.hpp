/*
 * taken from my fmt library
 * https://github.com/Naira0terminator/fmt/tree/master
 */

#pragma once

#include <string>
#include <cstdio>
#include <utility>
#include <sstream>

#include "../types/token.hpp"
#include "../value.hpp"
#include "../io.hpp"

namespace fmt
{
    template<typename T>
    concept is_iterable = requires(T t)
    {
        t.begin();
        t.end();
    };

    template<is_iterable T>
    std::string to_string(const T& container);

    template<typename A, typename B>
    std::string to_string(const std::pair<const A, B>& pair);

    inline std::string to_string(std::stringstream& ss)
    {
        return ss.str();
    }

    inline std::string to_string(std::nullptr_t ptr)
    {
        return "null";
    }

    inline std::string to_string(Bytes bytes)
    {
        return opcode_str[(uint8_t)bytes.code];
    }

    inline std::string to_string(bool b)
    {
        return b ? "true" : "false";
    }

    template<typename... A>
    std::string format(std::string_view fmt, A&&... a);

    inline std::string to_string(const Token& token)
    {
        return format("type({}) lexeme({}) position({}:{})",
                      tk_type_str[(uint8_t)token.type],
                      token.lexeme,
                      token.line, token.column);
    }

    inline std::string to_string(const Value& value)
    {
        return value.to_string();
    }

    template<typename T>
    inline std::string string_of(const T& value)
    {
        if constexpr(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
            return std::to_string(value);
        else if constexpr(
                std::is_constructible_v<std::string, T>
                && !std::is_same_v<T, std::nullptr_t>)
            return std::string{value};
        else
            return fmt::to_string(value);
    }

    template<typename A, typename B>
    inline std::string to_string(const std::pair<const A, B>& pair)
    {
        return (string_of(pair.first) + ": " + string_of(pair.second));
    }

    template<is_iterable T>
    std::string to_string(const T& container)
    {
        if(container.empty())
            return "[]";

        std::string result;

        if(container.size() == 1)
        {
            result += "[ " + string_of(*container.begin()) + " ]";
            return result;
        }

        auto current = container.begin();

        result += "[ " + string_of(*current);

        current++;

        for(; current != container.end(); current++)
            result += ", " + string_of(*current);

        result += " ]";

        return result;
    }

    template<typename... A>
    std::string format(std::string_view fmt, A&&... a)
    {
        if constexpr(sizeof...(a) == 0)
            return std::string{fmt};

        std::string buffer;

        ((buffer += string_of(a) + '\0'), ...);

        std::string output;
        size_t offset{};

        for(size_t i = 0; i < fmt.size(); i++)
        {
            if(fmt[i] == '{')
            {
                if(fmt[++i] == '{')
                    goto concat;

                for(char c = buffer[offset++]; c; c = buffer[offset++])
                {
                    output += c;
                }

                while(i < fmt.size() && fmt[i] != '}')
                    i++;
                continue;
            }

            concat:
            output += fmt[i];
        }

        return output;
    }

    template<typename... A>
    inline void print(std::string_view fmt, A&&... a)
    {
        mio::print(format(fmt, std::forward<A>(a)...));
        //return std::printf(format(fmt, std::forward<A>(a)...).data());
    }

    template<typename... A>
    inline int eprint(std::string_view fmt, A&&... a)
    {
        return std::fprintf(stderr, format(fmt, std::forward<A>(a)...).data());
    }

    template<typename... A>
    inline void fatal(std::string_view fmt, A&&... a)
    {
        eprint(fmt::format("{}", fmt), (a)...);
        std::exit(-1);
    }
}