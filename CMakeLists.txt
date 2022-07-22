cmake_minimum_required(VERSION 3.21)
project(strix)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Ot")

add_executable(strix
        src/main.cpp
        src/types/chunk.hpp
        src/util/debug.cpp src/util/debug.hpp
        src/vm.cpp src/vm.hpp
        src/compiler.cpp src/compiler.hpp
        src/scanner.cpp src/scanner.hpp
        src/util/util.hpp src/util/util.cpp
        src/value.hpp src/value.cpp
        src/data-structures/stack.hpp
        src/types/token.hpp
        src/util/fmt.hpp
        src/types/object.hpp
        src/objects/function.hpp
        src/objects/string.cpp src/objects/string.hpp
        src/io.cpp src/io.hpp
        src/objects/tuple.hpp
        src/objects/native_function.hpp)
