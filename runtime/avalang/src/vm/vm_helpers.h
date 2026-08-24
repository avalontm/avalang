#ifndef AVA_VM_VM_HELPERS_H
#define AVA_VM_VM_HELPERS_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

class ClassObj;

// Forward declare from value.h
struct Value;
struct Object;
enum class ValueType : avastd::uint8_t;

avastd::string GetFileDir(const avastd::string& path);
avastd::string NumberToString(double n);
avastd::string ValueToString(const Value& v);
bool ValueEquals(const Value& a, const Value& b);

// Nombre legible del tipo dinamico de un Value ("Number", "List", "Nil",
// etc.), usado en mensajes de error de tipo (type mismatch).
const char* ValueTypeName(ValueType t);

// Coercion numerica al estilo VB6 Variant, compartida por los operadores
// aritmeticos (vm_arith.cpp) y de comparacion relacional (vm_compare.cpp):
// un Number se usa tal cual; un String se acepta solo si -- recortado el
// whitespace -- parsea limpio como numero de punta a punta (CDbl/Val de
// VB6: "5" y "  3.5  " coercionan, "5px"/"hola" no); cualquier otro tipo
// (List, Dict, Function, Nil, Bool, Instance, etc.) no tiene lectura
// numerica razonable y tira runtime_error de "type mismatch" via
// AVA_THROW en vez de dejar que el caller lea `.n` sobre un union cuyo
// campo activo es otro. `op` es el operador para el mensaje de error
// (ej. "-", "<").
double CoerceToNumber(const Value& v, const char* op);
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