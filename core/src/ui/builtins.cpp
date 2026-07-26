#include "ui/builtins.h"

#include "vm/value.h"
#include "vm/vm.h"

#include <cstdio>
#include <string>

namespace ava {
namespace ui {

namespace {

// Minimal display formatter for ui.log's arguments. Deliberately NOT
// reusing the (more complete, recursive-into-list/dict) ToDisplayString
// from core/src/builtins/builtin_natives.cpp because that helper is
// file-local (anonymous namespace) there and not exposed via any
// header -- duplicating a small piece here is cheaper than exporting
// an internal helper across a build-order boundary for one caller.
// Covers the common cases (string/number/bool/nil); list/dict/etc.
// fall back to their type name, same degrade-gracefully spirit as
// EvalPropertyExpr's fallback in studio/src/design/state_eval.cpp.
std::string ToLogString(const Value& v) {
    switch (v.type) {
        case ValueType::Nil:
            return "nil";
        case ValueType::Bool:
            return v.b ? "true" : "false";
        case ValueType::Number: {
            double n = v.n;
            if (n == static_cast<long long>(n)) {
                return std::to_string(static_cast<long long>(n));
            }
            return std::to_string(n);
        }
        case ValueType::String:
            return static_cast<StringObj*>(v.obj)->data;
        case ValueType::List:
            return "<list>";
        case ValueType::Dict:
            return "<dict>";
        default:
            return "<value>";
    }
}

// ui.log(...) -- ver nota en builtins.h sobre por que es lo unico
// registrado en esta pasada. Misma forma que builtin_print
// (core/src/builtins/builtin_natives.cpp): junta los args con espacio,
// una sola llamada al print sink del host (ava_vm_set_print_callback),
// pero con prefijo "[ui] " para distinguirlo de print() en consola.
ava_value_t ui_log(AvaVM* vm, const ava_value_t* args, size_t count, void* /*user_data*/) {
    std::string line = "[ui] ";
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) line += ' ';
        line += ToLogString(FromC(args[i]));
    }
    line += '\n';

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) {
        raw_vm->Print(line);
    } else {
        std::fputs(line.c_str(), stdout);
    }

    Value nil = Value::Nil();
    return ToC(nil);
}

// ui.alert(msg) -- toma el primer argumento (formateado con ToLogString,
// igual que ui.log; msg vacío/faltante -> "") y lo pasa a VM::Alert, que
// lo enruta al alert sink que el host haya instalado (o degrada a Print,
// ver vm.cpp). Solo mira args[0] -- a diferencia de ui.log no tiene
// sentido "juntar" varios argumentos para un alert.
ava_value_t ui_alert(AvaVM* vm, const ava_value_t* args, size_t count, void* /*user_data*/) {
    std::string message = (count > 0) ? ToLogString(FromC(args[0])) : "";

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) raw_vm->Alert(message);

    Value nil = Value::Nil();
    return ToC(nil);
}

// ui.navigate(route) -- mismo shape que ui_alert pero enrutado a
// VM::Navigate. `route` es un string opaco para el core (ver builtins.h
// sobre por qué el core no interpreta rutas).
ava_value_t ui_navigate(AvaVM* vm, const ava_value_t* args, size_t count, void* /*user_data*/) {
    std::string route = (count > 0) ? ToLogString(FromC(args[0])) : "";

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) raw_vm->Navigate(route);

    Value nil = Value::Nil();
    return ToC(nil);
}

} // namespace

void RegisterUIBuiltins(AvaVM* vm) {
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (!raw_vm) return;

    // `ui` se registra como un Dict-como-namespace (no un native suelto)
    // para que `ui.log(...)` compile via GETGLOBAL "ui" + GETATTR "log"
    // -- ver builtins.h para por que RegisterNative("ui.log", ...) no
    // serviria (el compilador nunca emite un nombre global con punto).
    auto* ui_dict = new DictObj();

    auto add_native = [&](const char* name, AvaNativeFn fn) {
        auto* native = new NativeObj();
        native->fn = fn;
        native->user_data = nullptr;
        Value value;
        value.type = ValueType::Native;
        value.obj = native;
        ui_dict->index[name] = ui_dict->entries.size();
        ui_dict->entries.push_back({name, value});
    };

    add_native("log", ui_log);
    add_native("alert", ui_alert);
    add_native("navigate", ui_navigate);

    Value ui_value;
    ui_value.type = ValueType::Dict;
    ui_value.obj = ui_dict;

    raw_vm->SetGlobal("ui", ui_value);
}

} // namespace ui
} // namespace ava
