#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

inline std::string load_file (const std::string& path)
{
    std::ifstream file (path, std::ios::binary);
    if (!file)
        throw std::runtime_error ("Cannot open: " + path);
    return {std::istreambuf_iterator<char> (file), {}};
}
