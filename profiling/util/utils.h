/*
 * FileName: utils.h
 * Author: jattle
 * Copyright (c) 2024 jattle
 * Descrption: 
 */
#pragma once

#include <string>

namespace pprofcpp {

/// @brief get c++ ABI demangled name, if failed, return mangled name
std::string DemangleName(const char* mangled_name);
/// @brief load full content from file, return 0 if load successfully
int LoadFileContent(const std::string& filename, std::string* content);

/// @brief trim front and back characters of sv 
/// @param sv 
/// @return 
std::string_view Trim(std::string_view sv);

/// @brief trim front empty characters of sv 
/// @param sv 
/// @return 
std::string_view TrimFront(std::string_view sv);

/// @brief trim back empty characters of sv
/// @param sv 
/// @return 
std::string_view TrimBack(std::string_view sv);

}