// Copyright (c) 2016, Tamas Csala

#ifndef FILE_UTILS_HPP_
#define FILE_UTILS_HPP_

#include <string>
#include <fstream>
#include <sstream>

namespace FileUtils {

inline std::string ReadFileToString(const std::string& path) {
    std::ifstream shader_file(path.c_str());
    if (!shader_file.is_open()) {
      throw std::runtime_error("File '" + path + "' not found.");
    }
    std::stringstream shader_string;
    shader_string << shader_file.rdbuf();

    // Remove the EOF from the end of the string.
    std::string src = shader_string.str();
    if (src[src.length() - 1] == EOF) {
      src.pop_back();
    }

    return src;
}

}

#endif //FILE_UTILS_HPP_
