#include "vm.h"
#include "vm_internal.h"
#include "vm_platform_accessor.h"
#include "module.h"
#include "coroutine.h"
#include "../frontend/frontend.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace ava {

VM::VM() = default;

VM::~VM() {
    for (auto& kv : globals_) {
        Release(kv.second);
    }
    for (auto* co : created_coroutines_) {
        delete co;
    }
}

void VM::RegisterNative(const std::string& name, AvaNativeFn fn, void* user_data) {
    auto* native = new NativeObj();
    native->fn = fn;
    native->user_data = user_data;
    Value v;
    v.type = ValueType::Native;
    v.obj = native;
    SetGlobal(name, v);
}

void VM::RegisterBuiltinMethod(const std::string& name, AvaNativeFn fn, void* user_data) {
    builtin_methods_[name] = {fn, user_data};
}

void VM::SetPrintSink(PrintSink sink) {
    print_sink_ = std::move(sink);
}

void VM::Print(const std::string& text) const {
    if (print_sink_) {
        print_sink_(text);
    } else {
        std::fputs(text.c_str(), stdout);
    }
}

void VM::SetInputSink(InputSink sink) {
    input_sink_ = std::move(sink);
}

std::string VM::ReadLine(const std::string& prompt) const {
    if (input_sink_) {
        return input_sink_(prompt);
    }
    if (!prompt.empty()) {
        std::fputs(prompt.c_str(), stdout);
        std::fflush(stdout);
    }
    std::string line;
    bool ok = VmPlatformAccessor::Get().Console().ReadLine(line);
    if (!ok) return std::string();
    return line;
}

void VM::SetAlertSink(AlertSink sink) {
    alert_sink_ = std::move(sink);
}

void VM::Alert(const std::string& message) const {
    if (alert_sink_) {
        alert_sink_(message);
    } else {
        Print("[ui:alert] " + message + "\n");
    }
}

void VM::SetNavigateSink(NavigateSink sink) {
    navigate_sink_ = std::move(sink);
}

void VM::Navigate(const std::string& route) const {
    if (navigate_sink_) {
        navigate_sink_(route);
    } else {
        Print("[ui:navigate] " + route + " (sin router bindeado)\n");
    }
}

// Fase 4 (avapack): ver declaracion en vm.h. Sin hook instalado esto es
// un no-op -- DoImport (vm_import.cpp) llama estos getters directo (no
// necesitan degradar a ningun comportamiento por default como
// Print/Alert/Navigate, porque "no hacer nada" ya ES el comportamiento
// correcto de no tener el hook puesto).
void VM::SetBeforeModuleReadHook(ModuleFileHook hook) {
    before_module_read_hook_ = std::move(hook);
}

void VM::SetAfterModuleReadHook(ModuleFileHook hook) {
    after_module_read_hook_ = std::move(hook);
}

bool VM::HasBuiltinMethod(const std::string& name) const {
    return builtin_methods_.find(name) != builtin_methods_.end();
}

Value VM::GetBuiltinMethod(const std::string& name) const {
    auto it = builtin_methods_.find(name);
    if (it == builtin_methods_.end()) return Value::Nil();
    
    auto* native = new NativeObj();
    native->fn = it->second.first;
    native->user_data = it->second.second;
    Value v;
    v.type = ValueType::Native;
    v.obj = native;
    return v;
}

Value VM::GetGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    if (it == globals_.end()) {
        return Value::Nil();
    }
    // Every caller of ava_get_global (RuntimeHost::EvalPropertyExpr,
    // RuntimeHost::InvokeHandler, ExportComponentPropsNative, Ava
    // Studio's state_eval.cpp, ...) reads the value and then calls
    // ava_value_release on it, i.e. it treats this as handing over an
    // owned reference. globals_ itself keeps exactly one reference per
    // entry (SetGlobal Retains on insert/replace, Releases the value it
    // overwrites) -- returning that same reference here without
    // retaining lets the caller's later release drop the refcount to 0
    // and free an object the globals map still points to. The next
    // SetGlobal/GetGlobal on that name then touches freed memory
    // (dangling read, or a double-free via SetGlobal's own Release of
    // the "old" value), which is exactly the kind of heap corruption
    // that surfaces later as an unrelated-looking crash. Retain here so
    // the returned Value is a fresh reference the caller legitimately
    // owns and may release.
    Retain(it->second);
    return it->second;
}

void VM::SetGlobal(const std::string& name, Value value) {
    Retain(value);
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        Release(it->second);
        it->second = value;
    } else {
        globals_.emplace(name, value);
    }
}

Value VM::Run(const std::shared_ptr<Proto>& main) {
    const size_t base = frames_.size();
    CallFrame frame;
    frame.proto = main;
    frame.registers.resize(main->num_registers);
    frames_.push_back(std::move(frame));
    try {
        Value result = ExecuteFrame(frames_.size() - 1);
        frames_.pop_back();
        return result;
    } catch (...) {
        frames_.resize(base);
        throw;
    }
}

void VM::RaiseException(const Value& exc) {
    pending_exception_ = exc;
    try_had_exception_ = true;
}

Value VM::GetAndClearException() {
    Value exc = pending_exception_;
    pending_exception_ = Value::Nil();
    try_had_exception_ = false;
    return exc;
}

bool VM::HasException() const {
    return try_had_exception_;
}

Coroutine* VM::CreateCoroutine(const Value& func) {
    if (func.type != ValueType::Function) {
        throw std::runtime_error("attempt to create coroutine from non-function");
    }
    auto* closure = static_cast<Closure*>(func.obj);
    auto* co = new Coroutine();
    co->entry = func;
    Retain(co->entry);
    co->status = CoStatus::Suspended;
    created_coroutines_.push_back(co);
    return co;
}

} // namespace ava