#ifndef AVA_VM_VALUE_H
#define AVA_VM_VALUE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>

#include "ava.h"

namespace ava {

enum class ValueType : uint8_t {
    Nil = 0,
    Bool,
    Number,
    String,
    List,
    Dict,
    Function,
    Instance,
    Class,
    Coroutine,
    Native,
    Bound
};

struct Object {
    std::atomic<int64_t> ref_count{1};
    virtual ~Object() = default;
};

struct StringObj : Object {
    std::string data;
    explicit StringObj(std::string s) : data(std::move(s)) {}
};

struct Value; // fwd decl

struct ListObj : Object {
    std::vector<Value> items;
};

struct DictObj : Object {
    // Insertion-ordered map: vector of keys + hash index, mirrors Lua/Python
    // dict semantics well enough for a v1. Keys are always strings for now;
    // numeric keys can be added later without touching the C API surface.
    std::vector<std::pair<std::string, Value>> entries;
    std::unordered_map<std::string, size_t> index;
};

struct Proto;   // compiler/proto.h
struct Closure;
struct ClassObj;
struct Coroutine;
struct BoundMethod;

// Native (host) function pointer, matches AvaNativeFn from ava.h.
struct NativeObj : Object {
    AvaNativeFn fn;
    void* user_data;
    bool is_primitive_method = false;
    ava_value_t primitive_this{};
};

struct ClassObj : Object {
    std::string name;
    std::unordered_map<std::string, Value> attrs;
    std::unordered_map<std::string, std::shared_ptr<Proto>> methods;
    std::vector<std::string> param_names;
    ClassObj* base_class = nullptr;
};

struct InstanceObj : Object {
    ClassObj* cls;
    std::unordered_map<std::string, Value> attrs;
};

// A single dynamically-typed value. Ref-counted objects (String/List/Dict/
// Function/Instance/Class/Coroutine/Native) are stored as an Object* behind
// a manual ref count; Value itself is cheap to copy (16 bytes: tag + union).
struct Value {
    ValueType type = ValueType::Nil;
    union {
        bool     b;
        double   n;
        Object*  obj;
    };

    Value() : type(ValueType::Nil), n(0) {}
    static Value Nil()                 { Value v; v.type = ValueType::Nil; return v; }
    static Value Bool(bool x)          { Value v; v.type = ValueType::Bool; v.b = x; return v; }
    static Value Number(double x)      { Value v; v.type = ValueType::Number; v.n = x; return v; }

    bool IsTruthy() const {
        switch (type) {
            case ValueType::Nil:   return false;
            case ValueType::Bool:  return b;
            case ValueType::Number: return n != 0;
            default:               return true;
        }
    }

    bool IsRefCounted() const {
        switch (type) {
            case ValueType::String:
            case ValueType::List:
            case ValueType::Dict:
            case ValueType::Function:
            case ValueType::Instance:
            case ValueType::Class:
            case ValueType::Coroutine:
            case ValueType::Native:
            case ValueType::Bound:
                return true;
            default:
                return false;
        }
    }
};

struct BoundMethod : Object {
    std::shared_ptr<Proto> proto;
    Value instance;
};

void Retain(const Value& v);
void Release(const Value& v);

// Conversions to/from the public C ABI struct (ava_value_t). These are the
// only functions that should touch AvaRef.id <-> Object* directly.
ava_value_t ToC(const Value& v);
Value FromC(const ava_value_t& v);

} // namespace ava

#endif // AVA_VM_VALUE_H
