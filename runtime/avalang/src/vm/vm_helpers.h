#ifndef AVA_VM_VM_HELPERS_H
#define AVA_VM_VM_HELPERS_H

#include <string>
#include <cstddef>

namespace ava {

class ClassObj;

// Forward declare from value.h
struct Value;
struct Object;

std::string GetFileDir(const std::string& path);
std::string NumberToString(double n);
std::string ValueToString(const Value& v);
bool ValueEquals(const Value& a, const Value& b);
size_t ValidateIntegerIndex(double n, const char* context);
std::string JoinPath(const std::string& a, const std::string& b);
bool FileExists(const std::string& path);
std::string GetCurrentWorkingDir();
ClassObj* FindClassOwningAttr(ClassObj* cls, const std::string& name);

// VM conversion functions (defined in value.cpp)
ava_value_t ToC(const Value& v);
Value FromC(const ava_value_t& v);

} // namespace ava

#endif