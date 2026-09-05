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
#include "builtin_shared.h"
#include "vm/vm.h"
#include "vm/vm_platform_accessor.h"

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

using namespace ava;

namespace {

void* AsPtr(const Value& v) {
    if (v.type != ValueType::Number) return nullptr;
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int64_t>(v.n)));
}

double PtrToNumber(void* p) {
    return static_cast<double>(reinterpret_cast<int64_t>(p));
}

avastd::mutex g_alloc_mutex;
avastd::unordered_map<void*, size_t> g_allocated;

size_t AllocSize(void* ptr) {
    avastd::lock_guard<avastd::mutex> lock(g_alloc_mutex);
    auto it = g_allocated.find(ptr);
    if (it == g_allocated.end()) return 0;
    return it->second;
}

void TrackAlloc(void* ptr, size_t size) {
    if (!ptr) return;
    avastd::lock_guard<avastd::mutex> lock(g_alloc_mutex);
    g_allocated[ptr] = size;
}

void UntrackAlloc(void* ptr) {
    avastd::lock_guard<avastd::mutex> lock(g_alloc_mutex);
    g_allocated.erase(ptr);
}

char* CheckedPtr(const Value& v, size_t offset, size_t need) {
    void* base = AsPtr(v);
    if (!base) {
        throw avastd::runtime_error(
            "mem: expected a raw pointer (number from mem_alloc or a native return), got non-pointer");
    }
    size_t total = AllocSize(base);
    if (total == 0) {
        throw avastd::runtime_error(
            "mem: pointer passed is not a tracked mem_alloc buffer (alloc, then read/write; "
            "pointers from extern are read-only)");
    }
    if (offset > total || need > total - offset) {
        throw avastd::runtime_error(
            "mem: buffer out of bounds (offset=" + avastd::to_string(offset) +
            " size=" + avastd::to_string(need) + " but buffer is " + avastd::to_string(total) + " bytes)");
    }
    return static_cast<char*>(base) + offset;
}

double GetByteAt(char* p) {
    return static_cast<double>(static_cast<unsigned char>(*p));
}

double ArgNum(const ava_value_t* args, size_t i) {
    Value v = FromC(args[i]);
    if (v.type != ValueType::Number) {
        throw avastd::runtime_error("mem: expected a number argument");
    }
    return v.n;
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
    return ToCNew(Value::String(avastd::string(static_cast<const char*>(ptr))));
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

ava_value_t builtin_mem_peek_u32(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    void* base = AsPtr(FromC(args[0]));
    if (!base) return ToC(Value::Number(0));
    size_t off = static_cast<size_t>(ArgNum(args, 1));
    const unsigned char* p = static_cast<const unsigned char*>(base) + off;
    unsigned int val;
    avastd::memcpy(&val, p, 4);
    return ToC(Value::Number(val));
}

ava_value_t builtin_mem_alloc(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    Value v = FromC(args[0]);
    if (v.type != ValueType::Number || v.n < 0) {
        throw avastd::runtime_error("mem_alloc: size must be a non-negative number");
    }
    size_t size = static_cast<size_t>(v.n);
    if (size == 0) {
        throw avastd::runtime_error("mem_alloc: cannot allocate a 0-byte buffer");
    }
    char* mem = new char[size];
    for (size_t i = 0; i < size; ++i) mem[i] = 0;
    TrackAlloc(static_cast<void*>(mem), size);
    return ToC(Value::Number(PtrToNumber(mem)));
}

ava_value_t builtin_mem_free(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Nil());
    Value v = FromC(args[0]);
    void* ptr = AsPtr(v);
    if (ptr && AllocSize(ptr) != 0) {
        UntrackAlloc(ptr);
        delete[] static_cast<char*>(ptr);
    }
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_leak_count(AvaVM*, const ava_value_t* args, size_t count, void*) {
    (void)args; (void)count;
    avastd::lock_guard<avastd::mutex> lock(g_alloc_mutex);
    return ToC(Value::Number(static_cast<double>(g_allocated.size())));
}

ava_value_t builtin_mem_read_byte(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 1);
    return ToC(Value::Number(GetByteAt(p)));
}

ava_value_t builtin_mem_read_u16(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 2);
    unsigned short val;
    avastd::memcpy(&val, p, 2);
    return ToC(Value::Number(val));
}

ava_value_t builtin_mem_read_u32(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 4);
    unsigned int val;
    avastd::memcpy(&val, p, 4);
    return ToC(Value::Number(val));
}

ava_value_t builtin_mem_read_u64(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 8);
    unsigned long long val;
    avastd::memcpy(&val, p, 8);
    return ToC(Value::Number(static_cast<double>(val)));
}

ava_value_t builtin_mem_write_byte(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToC(Value::Nil());
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 1);
    *p = static_cast<char>(static_cast<unsigned char>(static_cast<int>(ArgNum(args, 2)) & 0xFF));
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_write_u16(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToC(Value::Nil());
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 2);
    unsigned short val = static_cast<unsigned short>(ArgNum(args, 2));
    avastd::memcpy(p, &val, 2);
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_write_u32(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToC(Value::Nil());
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 4);
    unsigned int val = static_cast<unsigned int>(ArgNum(args, 2));
    avastd::memcpy(p, &val, 4);
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_write_u64(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToC(Value::Nil());
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), 8);
    unsigned long long val = static_cast<unsigned long long>(ArgNum(args, 2));
    avastd::memcpy(p, &val, 8);
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_read_bytes(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) {
        Value empty; empty.type = ValueType::List; empty.obj = new ListObj();
        return ToCNew(empty);
    }
    Value base_v = FromC(args[0]);
    size_t offset = static_cast<size_t>(ArgNum(args, 1));
    size_t n = static_cast<size_t>(ArgNum(args, 2));
    char* p = CheckedPtr(base_v, offset, n);

    auto* list = new ListObj();
    for (size_t i = 0; i < n; ++i) {
        list->items.push_back(Value::Number(GetByteAt(p + i)));
    }
    Value out; out.type = ValueType::List; out.obj = list;
    return ToCNew(out);
}

ava_value_t builtin_mem_write_bytes(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToC(Value::Nil());
    Value base_v = FromC(args[0]);
    size_t offset = static_cast<size_t>(ArgNum(args, 1));
    Value payload = FromC(args[2]);

    char* p;
    size_t n;
    if (payload.type == ValueType::List) {
        auto* list = static_cast<ListObj*>(payload.obj);
        n = list->items.size();
        p = CheckedPtr(base_v, offset, n);
        for (size_t i = 0; i < n; ++i) {
            p[i] = static_cast<char>(static_cast<unsigned char>(static_cast<int>(AsNumber(list->items[i])) & 0xFF));
        }
    } else if (payload.type == ValueType::Number) {
        n = 1;
        p = CheckedPtr(base_v, offset, n);
        p[0] = static_cast<char>(static_cast<unsigned char>(static_cast<int>(AsNumber(payload)) & 0xFF));
    } else {
        throw avastd::runtime_error("mem_write_bytes: payload must be a List of bytes (0..255) or a number");
    }
    return ToC(Value::Nil());
}

ava_value_t builtin_mem_string(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 3) return ToCNew(Value::String(""));
    size_t n = static_cast<size_t>(ArgNum(args, 2));
    char* p = CheckedPtr(FromC(args[0]), static_cast<size_t>(ArgNum(args, 1)), n);
    return ToCNew(Value::String(avastd::string(p, n)));
}

} // extern "C"
