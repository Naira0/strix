#include "util.hpp"

#include <iostream>
#include <fstream>
#include <sys/stat.h>

inline size_t file_size(const char *filename)
{
    struct stat st;

    stat(filename, &st);

    return st.st_size;
}

std::optional<std::string> read_file(std::filesystem::path &&path)
{
    std::fstream file(path, std::fstream::in);

    if(!file.is_open())
        return std::nullopt;

    std::string output;

    size_t f_size = file_size(path.string().data());

    output.reserve(f_size);

    char c;

    while((c = file.get()) != EOF)
        output += c;

    return output;
}

std::string number_str(double value)
{
    std::string str = std::to_string(value);

    size_t pos = str.find('.');

    // this is not efficient but relative to the average string size it does the job ok
    if((int)value == value)
    {
        str = str.substr(0, pos);
    }
    else
    {
        bool found = false;

        for(size_t i = str.size()-1; i > pos; i--)
        {
            if(str[i] != '0')
            {
                found = true;
                pos = i+1;
                break;
            }
        }

        if(found)
            str = str.substr(0, pos);
    }

    return str;
}


