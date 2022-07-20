#pragma once

#include <string_view>

/*
 * mini io
 * a small io library to be used with the fmtlib and builtin functions
 * mio::print can be twice as fast as printf
 */

namespace mio
{
    void print(std::string_view message);
}
