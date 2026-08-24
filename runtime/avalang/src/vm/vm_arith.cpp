#include "vm.h"
#include "vm_internal.h"
#include "vm_helpers.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

void OpAdd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& Ra = frame.registers[in.a];
    auto& Rb = frame.registers[in.b];
    auto& Rc = frame.registers[in.c];
    if (Rb.type == ValueType::String || Rc.type == ValueType::String) {
        // Modernidad sobre VB6 a proposito: VB6 real separa '+' (numerico
        // puro) de '&' (concat siempre). Acá se mantiene '+' como en la
        // version actual de AvaLang -- si cualquiera de los dos lados es
        // String, concatena, sin necesidad de un operador aparte. No es
        // el fix de esta sesion; queda igual que estaba.
        avastd::string s1 = (Rb.type == ValueType::String)
            ? static_cast<StringObj*>(Rb.obj)->data : ValueToString(Rb);
        avastd::string s2 = (Rc.type == ValueType::String)
            ? static_cast<StringObj*>(Rc.obj)->data : ValueToString(Rc);
        Value v; v.type = ValueType::String; v.obj = new StringObj(s1 + s2);
        Ra = v;
    } else if (Rb.type == ValueType::List && Rc.type == ValueType::List) {
        auto* list1 = static_cast<ListObj*>(Rb.obj);
        auto* list2 = static_cast<ListObj*>(Rc.obj);
        auto* result = new ListObj();
        result->items.reserve(list1->items.size() + list2->items.size());
        // push_back copy-construye cada Value -> ya Retiene (RAII); el
        // Retain() manual que estaba antes de cada push_back duplicaba
        // esa retencion.
        for (auto& item : list1->items) {
            result->items.push_back(item);
        }
        for (auto& item : list2->items) {
            result->items.push_back(item);
        }
        Value v; v.type = ValueType::List; v.obj = result;
        Ra = v;
    } else {
        Ra = Value::Number(CoerceToNumber(Rb, "+") + CoerceToNumber(Rc, "+"));
    }
}

void OpSub(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        CoerceToNumber(frame.registers[in.b], "-") -
        CoerceToNumber(frame.registers[in.c], "-"));
}

void OpMul(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        CoerceToNumber(frame.registers[in.b], "*") *
        CoerceToNumber(frame.registers[in.c], "*"));
}

void OpDiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    double dividend = CoerceToNumber(frame.registers[in.b], "/");
    double divisor = CoerceToNumber(frame.registers[in.c], "/");
    if (divisor == 0.0) {
        AVA_THROW(avastd::runtime_error("division by zero"));
    }
    frame.registers[in.a] = Value::Number(dividend / divisor);
}

void OpIdiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    double dividend = CoerceToNumber(frame.registers[in.b], "//");
    double divisor = CoerceToNumber(frame.registers[in.c], "//");
    if (divisor == 0.0) {
        AVA_THROW(avastd::runtime_error("division by zero"));
    }
    frame.registers[in.a] = Value::Number(avastd::floor(dividend / divisor));
}

void OpMod(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    double dividend = CoerceToNumber(frame.registers[in.b], "%");
    double divisor = CoerceToNumber(frame.registers[in.c], "%");
    if (divisor == 0.0) {
        AVA_THROW(avastd::runtime_error("modulo by zero"));
    }
    // fmod() gives a C-style truncated remainder (sign follows the
    // dividend), but OpIdiv (//) above uses floor(a/b) -- a different
    // rounding convention. That mismatch broke the standard identity
    // (a // b) * b + (a % b) == a for negative operands. Adjust fmod's
    // result to floor-mod (sign follows the divisor) so % stays
    // consistent with //, matching Python's convention.
    double r = avastd::fmod(dividend, divisor);
    if (r != 0.0 && ((r < 0) != (divisor < 0))) {
        r += divisor;
    }
    frame.registers[in.a] = Value::Number(r);
}

void OpPow(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(avastd::pow(
        CoerceToNumber(frame.registers[in.b], "**"),
        CoerceToNumber(frame.registers[in.c], "**")));
}

void OpNeg(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        -CoerceToNumber(frame.registers[in.b], "-"));
}

void OpNot(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Bool(!frame.registers[in.b].IsTruthy());
}

void OpInc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        CoerceToNumber(frame.registers[in.b], "++") + 1);
}

void OpDec(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(
        CoerceToNumber(frame.registers[in.b], "--") - 1);
}

} // namespace ava