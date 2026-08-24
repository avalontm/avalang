#include "vm.h"
#include "vm_internal.h"
#include "vm_platform_accessor.h"
#include "module.h"
#include "coroutine.h"
#include "task.h"
#include "gc_sweep.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

VM::VM() = default;

VM::~VM() {

    for (auto* co : created_coroutines_) {
        delete co;
    }
    for (auto* task : created_tasks_) {
        delete task;
    }
}

void VM::CollectGcRoots(avastd::vector<Value*>& out) {
    out.reserve(out.size() + globals_.size());
    for (auto& kv : globals_) out.push_back(&kv.second);

    CollectFrameRoots(frames_, out);
    for (auto& saved : saved_frames_) CollectFrameRoots(saved, out);

    out.push_back(&pending_exception_);
    out.push_back(&yielded_values_);

    for (auto* co : created_coroutines_) {
        if (co) CollectCoroutineRoots(*co, out);
    }
    for (auto* task : created_tasks_) {
        if (task) CollectTaskRoots(*task, out);
    }
}

GcSweepStats VM::CollectGarbage() {
    return GcCollectCycles(*this);
}

void VM::RegisterNative(const avastd::string& name, AvaNativeFn fn, void* user_data) {
    auto* native = new NativeObj();
    native->fn = fn;
    native->user_data = user_data;
    Value v;
    v.type = ValueType::Native;
    v.obj = native;
    SetGlobal(name, v);
}

void VM::RegisterBuiltinMethod(const avastd::string& name, AvaNativeFn fn, void* user_data) {
    builtin_methods_[name] = {fn, user_data};
}

void VM::RegisterNativeModule(const avastd::string& module_path, NativeModuleFactory factory) {
    native_modules_[module_path] = avastd::move(factory);
}

void VM::SetPrintSink(PrintSink sink) {
    print_sink_ = avastd::move(sink);
}

void VM::Print(const avastd::string& text) const {
    if (print_sink_) {
        print_sink_(text);
    } else {
        VmPlatformAccessor::Get().Console().Write(text);
    }
}

void VM::SetInputSink(InputSink sink) {
    input_sink_ = avastd::move(sink);
}

avastd::string VM::ReadLine(const avastd::string& prompt) const {
    if (input_sink_) {
        return input_sink_(prompt);
    }
    if (!prompt.empty()) {
        VmPlatformAccessor::Get().Console().Write(prompt);
    }
    avastd::string line;
    bool ok = VmPlatformAccessor::Get().Console().ReadLine(line);
    if (!ok) return avastd::string();
    return line;
}

void VM::SetAlertSink(AlertSink sink) {
    alert_sink_ = avastd::move(sink);
}

void VM::Alert(const avastd::string& message) const {
    if (alert_sink_) {
        alert_sink_(message);
    } else {
        Print("[ui:alert] " + message + "\n");
    }
}

void VM::SetNavigateSink(NavigateSink sink) {
    navigate_sink_ = avastd::move(sink);
}

void VM::Navigate(const avastd::string& route) const {
    if (navigate_sink_) {
        navigate_sink_(route);
    } else {
        Print("[ui:navigate] " + route + " (sin router bindeado)\n");
    }
}

void VM::SetBeforeModuleReadHook(ModuleFileHook hook) {
    before_module_read_hook_ = avastd::move(hook);
}

void VM::SetAfterModuleReadHook(ModuleFileHook hook) {
    after_module_read_hook_ = avastd::move(hook);
}

bool VM::HasBuiltinMethod(const avastd::string& name) const {
    return builtin_methods_.find(name) != builtin_methods_.end();
}

Value VM::GetBuiltinMethod(const avastd::string& name) const {
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

Value VM::GetGlobal(const avastd::string& name) const {
    auto it = globals_.find(name);
    if (it == globals_.end()) {
        return Value::Nil();
    }
	
    return it->second;
}

void VM::SetGlobal(const avastd::string& name, Value value) {
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        it->second = avastd::move(value);
    } else {
        globals_.emplace(name, avastd::move(value));
    }
}

Value VM::Run(const avastd::shared_ptr<Proto>& main) {
    const size_t base = frames_.size();
    CallFrame frame;
    frame.proto = main;
    frame.registers.resize(main->num_registers);
    frames_.push_back(avastd::move(frame));
    AVA_TRY {
        Value result = ExecuteFrame(frames_.size() - 1);
        frames_.pop_back();
        return result;
    } AVA_CATCH(avastd::exception, e) {
        (void)e;
        frames_.resize(base);
        AVA_RETHROW();
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
        AVA_THROW(frames_.empty()
            ? AvaError("attempt to create coroutine from non-function")
            : MakeFrameError(frames_.back(), "attempt to create coroutine from non-function"));
    }
    auto* closure = static_cast<Closure*>(func.obj);
    auto* co = new Coroutine();
    co->entry = func;
    co->status = CoStatus::Suspended;
    created_coroutines_.push_back(co);
    return co;
}

} // namespace ava