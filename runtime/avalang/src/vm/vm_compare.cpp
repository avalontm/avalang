#include "vm.h"
#include "vm_internal.h"
#include "vm_helpers.h"

namespace ava {

void OpEq(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        Ra = Value::Bool(s1->data == s2->data);
    } else if (Rb.type == ValueType::Number && Rc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n == Rc.n);
    } else if (Rb.type == ValueType::Bool && Rc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b == Rc.b);
    } else if (Rb.type == ValueType::Nil && Rc.type == ValueType::Nil) {
        Ra = Value::Bool(true);
    } else if (Rb.type == ValueType::List && Rc.type == ValueType::List) {
        Ra = Value::Bool(ValueEquals(Rb, Rc));
    } else if (Rb.type == ValueType::Dict && Rc.type == ValueType::Dict) {
        Ra = Value::Bool(ValueEquals(Rb, Rc));
    } else {
        // Llegado aca, Rb/Rc no son String/Number/Bool/Nil/List/Dict (ya
        // cubiertos arriba) -- lo que queda (Function, Instance, Class,
        // Coroutine, Native, Bound, Exception, Module, Task) es siempre
        // Object-based: el campo activo del union es .obj, no .n. Antes
        // esto comparaba .n -- leyendo el puntero reinterpretado como
        // double en vez de comparar el puntero en si.
        Ra = Value::Bool(Rb.type == Rc.type && Rb.obj == Rc.obj);
    }
}

void OpEqK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    const Value& Kc = K[in.c];
    if (Rb.type == ValueType::Bool && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(!Rb.b);
    } else if (Rb.type == ValueType::String && Kc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Kc.obj);
        Ra = Value::Bool(s1->data == s2->data);
    } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n == Kc.n);
    } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b == Kc.b);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
        Ra = Value::Bool(true);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(true);
    } else {
        // Ver el comentario equivalente en OpEq: mismo fix, .obj en vez
        // de .n para el resto de tipos Object-based.
        Ra = Value::Bool(Rb.type == Kc.type && Rb.obj == Kc.obj);
    }
}

void OpNeK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    const Value& Kc = K[in.c];
    if (Rb.type == ValueType::Bool && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(Rb.b);
    } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n != Kc.n);
    } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b != Kc.b);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
        Ra = Value::Bool(false);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(false);
    } else {
        // Ver el comentario equivalente en OpEq: mismo fix, .obj en vez
        // de .n para el resto de tipos Object-based.
        Ra = Value::Bool(!(Rb.type == Kc.type && Rb.obj == Kc.obj));
    }
}

void OpNe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        Ra = Value::Bool(s1->data != s2->data);
    } else if (Rb.type == ValueType::Number && Rc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n != Rc.n);
    } else if (Rb.type == ValueType::Bool && Rc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b != Rc.b);
    } else if (Rb.type == ValueType::Nil && Rc.type == ValueType::Nil) {
        Ra = Value::Bool(false);
    } else if (Rb.type == ValueType::List && Rc.type == ValueType::List) {
        Ra = Value::Bool(!ValueEquals(Rb, Rc));
    } else if (Rb.type == ValueType::Dict && Rc.type == ValueType::Dict) {
        Ra = Value::Bool(!ValueEquals(Rb, Rc));
    } else {
        // Ver el comentario equivalente en OpEq: mismo fix, .obj en vez
        // de .n para el resto de tipos Object-based.
        Ra = Value::Bool(!(Rb.type == Rc.type && Rb.obj == Rc.obj));
    }
}

// Rb.n / Rc.n solo son lectura valida cuando el Value activo es
// Number (el union tiene .n activo). Antes, la rama "no String" de los
// cuatro comparadores de abajo leia .n sin chequear el tipo -- exactamente
// el mismo bug que tenian OpSub/OpMul/OpDiv/etc. en vm_arith.cpp: un
// `[1,2] < 3` o un `nil >= 5` leian memoria de otro campo del union como
// si fuera double, sin error. CoerceToNumber (compartido con
// vm_arith.cpp via vm_helpers.h) aplica la misma regla estilo VB6 acá:
// Number tal cual, String numerica se parsea, cualquier otro tipo tira
// "type mismatch" real.

void OpLt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data < s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(
            CoerceToNumber(Rb, "<") < CoerceToNumber(Rc, "<"));
    }
}

void OpLe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data <= s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(
            CoerceToNumber(Rb, "<=") <= CoerceToNumber(Rc, "<="));
    }
}

void OpGt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data > s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(
            CoerceToNumber(Rb, ">") > CoerceToNumber(Rc, ">"));
    }
}

void OpGe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data >= s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(
            CoerceToNumber(Rb, ">=") >= CoerceToNumber(Rc, ">="));
    }
}

} // namespace ava