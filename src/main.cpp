#include <iostream>
#include <string>

#include "vm.hpp"
#include "types/chunk.hpp"
#include "compiler.hpp"
#include "util/util.hpp"
#include "util/fmt.hpp"
#include "util/debug.hpp"

/*
 * TODO:
 * add bitwise operators
 * string escaping
 * anonymous functions
 * multiple return values is very slow investigate why
 */

void print_tokens(char *path)
{
    auto contents = read_file(path);

    if(!contents.has_value())
        fmt::fatal("could not read input file");

    Scanner scanner(contents.value());

    Token token;

    using enum class TokenType;

    while((token = scanner.scan_token()).type != Eof)
    {
        if(!scanner.state.ok)
            fmt::fatal("{}", scanner.state.message);

        fmt::print("{}\n", token);

        if(token.type == FStringStart)
        {
            while(token.type != Eof && token.type != FStringEnd)
            {
                token = scanner.scan_fstring();
                fmt::print("{}\n", token);
            }
        }
    }
}

void print_bytes(const char *path)
{
    Chunk chunk;

    auto contents = read_file(path);

    if(!contents.has_value())
        fmt::fatal("could not read input file");

    Compiler compiler(contents.value());

    compiler.compile();

    disassemble_chunk(chunk, "current chunk");
}

// currently, things that rely on the source code like identifiers or static strings
// or even token lexemes will break with repl
void repl()
{
    VM vm;

    std::string line;

    while(true)
    {
        std::getline(std::cin, line);

        if(line.empty())
            continue;

        InterpretResult result = vm.interpret(line);
    }
}

void run_file(const char *path)
{
    auto contents = read_file(path);

    if(!contents.has_value())
        fmt::fatal("could not read input file");

    VM vm;

    InterpretResult result = vm.interpret(contents.value());
}


int main(int argc, char **argv)
{
    if(argc >= 2)
        run_file(argv[1]);
    else
        repl();

    //print_tokens(argv[1]);
    // print_bytes(argv[1]);
}
