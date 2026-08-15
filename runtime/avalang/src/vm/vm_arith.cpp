#include "vm.h"
#include "vm_internal.h"
#include "vm_helpers.h"
#include <cmath>
#include <stdexcept>

namespace ava {

void OpAdd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String || Rc.type == ValueType::String) {
        std::string s1 = (Rb.type == ValueType::String)
            ? static_cast<StringObj*>(Rb.obj)->data : ValueToString(Rb);
        std::string s2 = (Rc.type == ValueType::String)
            ? static_cast<StringObj*>(Rc.obj)->data : ValueToString(Rc);
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
    double divisor = frame.registers[in.c].n;
    if (divisor == 0.0) {
        throw std::runtime_error("division by zero");
    }
    frame.registers[in.a] = Value::Number(
        frame.registers[in.b].n / divisor);
}

void OpIdiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    double divisor = frame.registers[in.c].n;
    if (divisor == 0.0) {
        throw std::runtime_error("division by zero");
    }
    frame.registers[in.a] = Value::Number(
        std::floor(frame.registers[in.b].n / divisor));
}

void OpMod(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    double divisor = frame.registers[in.c].n;
    if (divisor == 0.0) {
        throw std::runtime_error("modulo by zero");
    }
    // fmod() gives a C-style truncated remainder (sign follows the
    // dividend), but OpIdiv (//) above uses floor(a/b) -- a different
    // rounding convention. That mismatch broke the standard identity
    // (a // b) * b + (a % b) == a for negative operands. Adjust fmod's
    // result to floor-mod (sign follows the divisor) so % stays
    // consistent with //, matching Python's convention.
    double r = std::fmod(frame.registers[in.b].n, divisor);
    if (r != 0.0 && ((r < 0) != (divisor < 0))) {
        r += divisor;
    }
    frame.registers[in.a] = Value::Number(r);
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