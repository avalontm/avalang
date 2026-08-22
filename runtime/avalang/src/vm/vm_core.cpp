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
    // Sub-fase 3 (Fase 5 GC): globals_ es unordered_map<string, Value>;
    // su propio destructor (llamado automaticamente despues de este
    // cuerpo) ya destruye cada Value y por lo tanto ya llama Release()
    // sobre cada uno via ~Value(). El loop manual que estaba aca antes
    // liberaba lo mismo una segunda vez -- doble release en cada global
    // al apagar la VM.
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

    // created_coroutines_/created_tasks_ acumulan TODO Coroutine/TaskObj
    // creado en la vida de la VM (nunca se saca nada de ahi salvo en
    // ~VM(), ver arriba) -- cubre current_coroutine_ y cada entrada de
    // coroutine_resumers_/TaskObj::awaiters sin tener que recorrerlos
    // aparte, porque cualquier Coroutine* ahi ya fue empujado a
    // created_coroutines_ al crearse (vm_core.cpp::CreateCoroutine,
    // vm_task.cpp).
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

void VM::SetPrintSink(PrintSink sink) {
    print_sink_ = avastd::move(sink);
}

void VM::Print(const avastd::string& text) const {
    if (print_sink_) {
        print_sink_(text);
    } else {
        // Antes: std::fputs(text.c_str(), stdout). <cstdio>/FILE* es I/O
        // de libc -- no existe en este kernel. Va por IConsole (misma
        // interfaz que ya usaba ReadLine() de aca abajo), consistente con
        // el resto del PAL.
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
        // Antes: std::fputs + std::fflush -- mismo motivo que Print()
        // arriba, va por IConsole. No hace falta "fflush" explicito:
        // Console().Write() es la unica via de salida en este PAL, no hay
        // buffering de libc que drenar (eso era una preocupacion
        // especifica de FILE* stdio, que ya no aplica aca).
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

// Fase 4 (avapack): ver declaracion en vm.h. Sin hook instalado esto es
// un no-op -- DoImport (vm_import.cpp) llama estos getters directo (no
// necesitan degradar a ningun comportamiento por default como
// Print/Alert/Navigate, porque "no hacer nada" ya ES el comportamiento
// correcto de no tener el hook puesto).
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
    // that surfaces later as an unrelated-looking crash. `return
    // it->second;` copy-constructs the returned Value from a const&,
    // and Value's own copy constructor (sub-fase 2, RAII) already
    // Retains -- that single automatic retain is exactly the fresh
    // reference the caller legitimately owns and may release. A manual
    // Retain() here on top of that would leak one ref per call.
    return it->second;
}

void VM::SetGlobal(const avastd::string& name, Value value) {
    // Sub-fase 3: `value` llega por copia -- el caller ya pago un
    // Retain al construir este parametro -- asi que esta funcion ya es
    // dueña de una referencia propia. Moverla directo al slot de
    // globals_ (en vez de copiarla con un Retain manual encima) deja a
    // globals_ con exactamente una referencia por entrada, tal como
    // documenta GetGlobal() arriba, sin retener de mas. El reemplazo
    // libera el valor viejo una sola vez via el move-assignment de
    // Value; el Release() manual que habia antes duplicaba esa
    // liberacion (double-release sobre la entrada vieja).
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
        AVA_THROW(avastd::runtime_error("attempt to create coroutine from non-function"));
    }
    auto* closure = static_cast<Closure*>(func.obj);
    auto* co = new Coroutine();
    // Sub-fase 3: co->entry es Value (coroutine.h), asi que esta
    // copy-assignment ya Retiene automaticamente (sub-fase 2, RAII). El
    // Retain() manual que seguia despues era una segunda retencion de
    // mas sobre el mismo Value.
    co->entry = func;
    co->status = CoStatus::Suspended;
    created_coroutines_.push_back(co);
    return co;
}

} // namespace ava