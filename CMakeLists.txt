cmake_minimum_required(VERSION 3.21)
project(strix)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Ot")

add_executable(strix
        src/main.cpp
        src/chunk.hpp
        src/debug.cpp src/debug.hpp
        src/vm.cpp src/vm.hpp
        src/compiler.cpp src/compiler.hpp
        src/scanner.cpp src/scanner.hpp
        src/util.hpp src/util.cpp
        src/value.hpp src/value.cpp
        src/stack.hpp
        src/token.hpp
        src/fmt.hpp
        src/object.hpp src/object.cpp)
