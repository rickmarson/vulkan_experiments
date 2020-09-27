/*
* file_system.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include <fstream>
#include <iostream>
#include <string>


inline std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        return std::vector<char>{};
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
