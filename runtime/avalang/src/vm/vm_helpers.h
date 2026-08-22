#ifndef AVA_VM_VM_HELPERS_H
#define AVA_VM_VM_HELPERS_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

class ClassObj;

// Forward declare from value.h
struct Value;
struct Object;

avastd::string GetFileDir(const avastd::string& path);
avastd::string NumberToString(double n);
avastd::string ValueToString(const Value& v);
bool ValueEquals(const Value& a, const Value& b);
size_t ValidateIntegerIndex(double n, const char* context);
avastd::string JoinPath(const avastd::string& a, const avastd::string& b);
bool FileExists(const avastd::string& path);
avastd::string GetCurrentWorkingDir();
ClassObj* FindClassOwningAttr(ClassObj* cls, const avastd::string& name);

// VM conversion functions (defined in value.cpp)
ava_value_t ToC(const Value& v);
Value FromC(const ava_value_t& v);

} // namespace ava

#endif