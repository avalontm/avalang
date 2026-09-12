#ifndef AVA_VM_VM_HELPERS_H
#define AVA_VM_VM_HELPERS_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

class ClassObj;

struct Value;
struct Object;
enum class ValueType : avastd::uint8_t;

avastd::string GetFileDir(const avastd::string& path);
avastd::string GetModuleBareName(const avastd::string& path);
avastd::string NumberToString(double n);
avastd::string ValueToString(const Value& v);
bool ValueEquals(const Value& a, const Value& b);

const char* ValueTypeName(ValueType t);

double CoerceToNumber(const Value& v, const char* op);

size_t ValidateIntegerIndex(double n, size_t len, const char* context);
avastd::string JoinPath(const avastd::string& a, const avastd::string& b);
avastd::string GetCurrentWorkingDir();
ClassObj* FindClassOwningAttr(ClassObj* cls, const avastd::string& name);

ava_value_t ToC(const Value& v);
Value FromC(const ava_value_t& v);

} // namespace ava

#endif