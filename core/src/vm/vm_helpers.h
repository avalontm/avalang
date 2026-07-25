#ifndef AVA_VM_VM_HELPERS_H
#define AVA_VM_VM_HELPERS_H

#include <string>
#include <cstddef>

namespace ava {

std::string GetFileDir(const std::string& path);
std::string NumberToString(double n);
size_t ValidateIntegerIndex(double n, const char* context);
std::string JoinPath(const std::string& a, const std::string& b);
bool FileExists(const std::string& path);
std::string GetCurrentWorkingDir();

}

#endif