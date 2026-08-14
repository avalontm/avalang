#include "ui/builtins.h"

#include "vm/value.h"
#include "vm/vm.h"

#include <cstdio>
#include <string>

namespace ava {
namespace ui {

namespace {










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






ava_value_t ui_log(AvaVM* vm, const ava_value_t* args, size_t count, void* ) {
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






ava_value_t ui_alert(AvaVM* vm, const ava_value_t* args, size_t count, void* ) {
    std::string message = (count > 0) ? ToLogString(FromC(args[0])) : "";

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) raw_vm->Alert(message);

    Value nil = Value::Nil();
    return ToC(nil);
}




ava_value_t ui_navigate(AvaVM* vm, const ava_value_t* args, size_t count, void* ) {
    std::string route = (count > 0) ? ToLogString(FromC(args[0])) : "";

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) raw_vm->Navigate(route);

    Value nil = Value::Nil();
    return ToC(nil);
}

}

void RegisterUIBuiltins(AvaVM* vm) {
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (!raw_vm) return;





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

}
}