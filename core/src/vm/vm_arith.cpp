#include "vm.h"
#include "vm_internal.h"
#include <cmath>

namespace ava {

void OpAdd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String || Rc.type == ValueType::String) {
        std::string s1 = Rb.type == ValueType::String
            ? static_cast<StringObj*>(Rb.obj)->data : NumberToString(Rb.n);
        std::string s2 = Rc.type == ValueType::String
            ? static_cast<StringObj*>(Rc.obj)->data : NumberToString(Rc.n);
        Value v; v.type = ValueType::String; v.obj = new StringObj(s1 + s2);
        Ra = v;
    } else if (Rb.type == ValueType::List && Rc.type == ValueType::List) {
        auto* list1 = static_cast<ListObj*>(Rb.obj);
        auto* list2 = static_cast<ListObj*>(Rc.obj);
        auto* result = new ListObj();
        result->items.reserve(list1->items.size() + list2->items.size());
        for (auto& item : list1->items) {
            Retain(item);
            result->items.push_back(item);
        }
        for (auto& item : list2->items) {
            Retain(item);
            result->items.push_back(item);
        }
        Value v; v.type = ValueType::List; v.obj = result;
        Ra = v;
    } else {
        Ra = Value::Number(Rb.n + Rc.n);
    }
}

void OpSub(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        frame.registers[in.b].n - frame.registers[in.c].n);
}

void OpMul(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        frame.registers[in.b].n * frame.registers[in.c].n);
}

void OpDiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        frame.registers[in.b].n / frame.registers[in.c].n);
}

void OpIdiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        std::floor(frame.registers[in.b].n / frame.registers[in.c].n));
}

void OpMod(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        std::fmod(frame.registers[in.b].n, frame.registers[in.c].n));
}

void OpPow(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        std::pow(frame.registers[in.b].n, frame.registers[in.c].n));
}

void OpNeg(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(-frame.registers[in.b].n);
}

void OpNot(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Bool(!frame.registers[in.b].IsTruthy());
}

void OpInc(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(frame.registers[in.b].n + 1);
}

void OpDec(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(frame.registers[in.b].n - 1);
}

} // namespace ava