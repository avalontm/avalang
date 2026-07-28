#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpEq(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
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
    } else {
        Ra = Value::Bool(Rb.type == Rc.type && Rb.n == Rc.n);
    }
}

void OpEqK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    const Value& Kc = K[in.c];
    if (Rb.type == ValueType::String && Kc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Kc.obj);
        Ra = Value::Bool(s1->data == s2->data);
    } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n == Kc.n);
    } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b == Kc.b);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
        Ra = Value::Bool(true);
    } else {
        Ra = Value::Bool(Rb.type == Kc.type && Rb.n == Kc.n);
    }
}

void OpNeK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    const Value& Kc = K[in.c];
    if (Rb.type == ValueType::Bool && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(!Rb.b);
    } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
        Ra = Value::Bool(Rb.n != Kc.n);
    } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
        Ra = Value::Bool(Rb.b != Kc.b);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
        Ra = Value::Bool(false);
    } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Number && Kc.n == 0) {
        Ra = Value::Bool(true);
    } else {
        Ra = Value::Bool(!(Rb.type == Kc.type && Rb.n == Kc.n));
    }
}

void OpNe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
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
    } else {
        Ra = Value::Bool(!(Rb.type == Rc.type && Rb.n == Rc.n));
    }
}

void OpLt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data < s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(Rb.n < Rc.n);
    }
}

void OpLe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data <= s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(Rb.n <= Rc.n);
    }
}

void OpGt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data > s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(Rb.n > Rc.n);
    }
}

void OpGe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
        auto* s1 = static_cast<StringObj*>(Rb.obj);
        auto* s2 = static_cast<StringObj*>(Rc.obj);
        frame.registers[in.a] = Value::Bool(s1->data >= s2->data);
    } else {
        frame.registers[in.a] = Value::Bool(Rb.n >= Rc.n);
    }
}

} // namespace ava