// Helpers de memoria cruda para usar junto con `extern` (ver
// vm/vm_extern.h/.cpp). El FFI actual solo sabe devolver numbers/bool
// (todo retorno se interpreta como int64 -> double, ver limitaciones en
// vm_extern.h), asi que una funcion nativa que en C retorna `char*`
// (p.ej. mysql_error) o un `char**` (p.ej. MYSQL_ROW) nos llega como un
// numero (la direccion). Estas tres funciones son el complemento minimo
// para poder leer lo que ese puntero apunta desde AvaLang:
//
//   mem_is_null(ptr)       -- true si ptr es NULL/nil
//   mem_peek_string(ptr)   -- lee un char* como string de AvaLang
//   mem_peek_ptr(base, i)  -- lee el puntero #i de un arreglo de
//                             punteros en `base` (para decodificar
//                             MYSQL_ROW: mem_peek_ptr(row, i) da la
//                             direccion de la columna i)
//
// Precision: una direccion de puntero cabe exacta en un double mientras
// no supere 2^53 -- cierto en la practica para cualquier direccion de
// heap de proceso de usuario en Windows/Linux/macOS x64 (ver la misma
// asuncion documentada en vm_extern.cpp para el valor de retorno).
//
// Esto es intencionalmente "unsafe": no valida que `ptr` sea una
// direccion legitima. Un puntero basura de un extern mal usado puede
// crashear el proceso igual que en C. Es el precio de la interoperacion
// real con librerias nativas que no se pensaron para este lenguaje.
#include "builtin_natives.h"

#include "vm/value.h"

#include <cstdint>
#include <string>

using namespace ava;

namespace {

void* AsPtr(const Value& v) {
    if (v.type != ValueType::Number) return nullptr;
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int64_t>(v.n)));
}

double PtrToNumber(void* p) {
    return static_cast<double>(reinterpret_cast<int64_t>(p));
}

} // namespace

extern "C" {

ava_value_t builtin_mem_is_null(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Bool(true));
    Value v = FromC(args[0]);
    if (v.type == ValueType::Nil) return ToC(Value::Bool(true));
    return ToC(Value::Bool(AsPtr(v) == nullptr));
}

ava_value_t builtin_mem_peek_string(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Nil());
    void* ptr = AsPtr(FromC(args[0]));
    if (!ptr) return ToC(Value::Nil());
    return ToC(Value::String(std::string(static_cast<const char*>(ptr))));
}

ava_value_t builtin_mem_peek_ptr(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Nil());
    void* base = AsPtr(FromC(args[0]));
    if (!base) return ToC(Value::Nil());
    Value idx_v = FromC(args[1]);
    size_t idx = idx_v.type == ValueType::Number ? static_cast<size_t>(idx_v.n) : 0;

    void** arr = static_cast<void**>(base);
    void* elem = arr[idx];
    if (!elem) return ToC(Value::Nil());
    return ToC(Value::Number(PtrToNumber(elem)));
}

} // extern "C"
