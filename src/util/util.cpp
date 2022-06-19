#include "util.hpp"

#include <iostream>

std::string read_file(const char *path)
{
    std::fstream file(path, std::fstream::in);

    if(!file.is_open())
        return {};

    return {
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()};
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