
#include "io.hpp"

#if defined(WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#endif

#if defined(WIN32)
HANDLE STDOUT_H = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

void mio::print(std::string_view message)
{
#if defined(WIN32)
    WriteConsole(STDOUT_H, message.data(), message.size(), 0, nullptr);
#elif defined(__linux__)
    syscall(4, STDOUT_FILENO, message, message.size());
#else
    std::cout << message;
#endif
}
