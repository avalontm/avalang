#ifndef AVA_VM_VM_H
#define AVA_VM_VM_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

#include "value.h"
#include "proto.h"
#include "../common/ava_error.h"
#include "closure.h"
#include "module.h"
#include "coroutine.h"
#include "task.h"
#include "gc_sweep.h"
#include "vm_helpers.h"
#include "../../api/include/avalang.h"

#ifdef _WIN32
  #define AVA_VM_API __declspec(dllexport)
#else
  #define AVA_VM_API __attribute__((visibility("default")))
#endif

namespace ava {

class AVA_VM_API VM {
public:
    VM();
    ~VM();

    void RegisterNative(const avastd::string& name, AvaNativeFn fn, void* user_data);
    void RegisterBuiltinMethod(const avastd::string& name, AvaNativeFn fn, void* user_data);

    // Phase 1 of AVALANG_IMPORT_SYSTEM_PLAN.md: lets an embedder (a
    // builtins/*.cpp registration function, not user script code) expose
    // a module by exact name that `import <module_path>` can resolve
    // without touching disk. `factory` is invoked once per `DoImport`
    // call for that name and must return a Dict value (built the same
    // way a builtin registration function already builds native
    // functions, see builtins/system_module.cpp) -- the same value that
    // `import <name>` would otherwise have built out of a compiled
    // module's globals_. No caching happens here: every `import` of a
    // registered name re-invokes `factory`.
    using NativeModuleFactory = avastd::function<Value(VM&)>;
    void RegisterNativeModule(const avastd::string& module_path, NativeModuleFactory factory);

    Value GetGlobal(const avastd::string& name) const;
    void  SetGlobal(const avastd::string& name, Value value);
    bool  HasGlobal(const avastd::string& name) const { return globals_.find(name) != globals_.end(); }

    // Busca, entre los modulos nativos registrados (RegisterNativeModule --
    // "system", "system.console", etc.), el primero cuyo Dict de nivel
    // superior expone una entrada llamada `symbol`. Construye cada Dict
    // de forma transitoria (invocando la factory sin llamar a
    // PlaceModuleInScope) solo para inspeccionarlo -- las factories
    // registradas en este codebase (ver builtins/system_module.cpp) son
    // puro armado de Value/Dict, sin efectos secundarios, asi que esto es
    // seguro de llamar desde un path de error. Devuelve el module_path
    // (p.ej. "system") o "" si ningun modulo nativo expone ese nombre.
    avastd::string FindNativeModuleExporting(const avastd::string& symbol) const;

    // Builds an AvaError stamped with the line/column/source of whatever
    // instruction is currently executing in the topmost frame (mirrors
    // MakeFrameError in vm_errors.cpp, but public so builtins/*.cpp --
    // native C functions that only get an opaque AvaVM* -- can report a
    // properly-located runtime error too, instead of throwing a bare
    // AvaError(msg) with line=0/column=0 that leaves the user with no way
    // to find which line caused it. Safe to call from inside a native's
    // AvaNativeFn body: at that point frames_ always has at least the
    // frame that issued the CALL/BASECALL, with pc already past it (same
    // indexing MakeFrameError relies on).
    AvaError MakeCurrentError(const avastd::string& message) const;

    Value Run(const avastd::shared_ptr<Proto>& main);
    Value RunFile(const avastd::string& file_path);

    Value Call(const Value& callable, const avastd::vector<Value>& args);
    
    bool HasBuiltinMethod(const avastd::string& name) const;
    Value GetBuiltinMethod(const avastd::string& name) const;

    avastd::unordered_map<avastd::string, Value>& Globals() { return globals_; }

    ModuleResolver& GetModuleResolver() { return module_resolver_; }
    ModuleCache& GetModuleCache() { return module_cache_; }

    avastd::string GetCurrentDir() const;
    void SetCurrentDir(const avastd::string& dir);
    Value DoImport(const avastd::string& module_path, const avastd::string& alias);

    void RaiseException(const Value& exc);
    Value GetAndClearException();
    bool HasException() const;
    size_t GetCurrentFrameIndex() const { return frames_.size() - 1; }

    Coroutine* CreateCoroutine(const Value& func);

    // Diagnostics para Fase 5 (GC) del plan de runtime independiente de
    // STL: bajo el modelo actual, Coroutine/TaskObj no participan del
    // refcounting de Object (ver value.h) -- viven mientras vive la VM,
    // registrados en created_coroutines_/created_tasks_, que hacen de
    // root set de facto para estos dos tipos. Estos accessors dejan
    // observar ese conteo desde fuera (host/BareKernel) sin exponer los
    // vectores internos; útiles para detectar crecimiento sin límite en
    // procesos de larga duración mientras no exista un colector real.
    // Ver docs/architecture/RUNTIME_CORE_AUDIT.md §9.
    size_t LiveCoroutineCount() const { return created_coroutines_.size(); }
    size_t LiveTaskCount() const { return created_tasks_.size(); }

    // Sub-fase 5 de Fase 5 (GC), "Crear roots" -- ver
    // docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §6. Junta un puntero
    // a cada Value que es raíz para el futuro tracing/cycle collector:
    // globals_, cada registro de cada CallFrame en frames_/saved_frames_
    // (stack actual y stacks de corrutinas/tasks anidados suspendidos a
    // mitad de un await), pending_exception_, yielded_values_, y las
    // frames/yielded_values/result/error de cada Coroutine/TaskObj vivo
    // (created_coroutines_/created_tasks_ -- éstos no participan del
    // refcounting de Object, ver el comentario en LiveCoroutineCount()
    // arriba, así que sus Value son raíces igual que un global). No
    // recorre containers (ListObj::items, etc.) -- eso es tracing
    // (próxima sub-fase), no roots. Los punteros son válidos solo hasta
    // el próximo cambio de tamaño de cualquiera de esos vectores/mapas;
    // pensado para usarse y descartarse dentro de un mismo ciclo de GC.
    void CollectGcRoots(avastd::vector<Value*>& out);

    // Sub-fase 7 de Fase 5 (GC), "Manejar ciclos" -- ver
    // docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md y gc_sweep.h. Corre
    // un ciclo completo de mark-sweep (CollectGcRoots + GcTraceMark +
    // GcForEachObject) y libera lo que quede sin marcar. NO se dispara
    // automatico en ningun punto todavia: durante la ejecucion de un
    // opcode puede haber Value temporarios solo en la pila nativa de C++
    // (no en frames_, no en roots) que un sweep a mitad de ejecucion
    // veria como basura y liberaria de mas -- es una decision real, no un
    // olvido, dejarla manual en vez de cablear un auto-trigger no
    // auditado. Pensado para invocarse entre ejecuciones completas (o via
    // ava_vm_collect_garbage() desde el host) donde no hay temporarios de
    // pila nativa vivos.
    GcSweepStats CollectGarbage();

    // Fase 5 (Async Runtime). Dispatcher simple sobre ITimer (ver
    // core/platform/interfaces/ITimer.h): set_timeout() agenda `callback`
    // para reejecutarse tras delayMs. El timer del PAL dispara en un hilo
    // del backend (WinTimer: su propio worker thread) -- NO es seguro
    // llamar directo a vm->Call() ahi (la VM no es reentrante entre
    // hilos). En cambio el callback del timer solo empuja a una cola
    // thread-safe (PostAsyncTask); PumpAsyncEvents() la drena y hace los
    // Call() reales desde el hilo que corre la VM (ver RunFile /
    // public/src/main.cpp, que hace el loop de "event loop" tras el
    // script principal).
    void PostAsyncTask(avastd::function<void()> task);
    void PumpAsyncEvents();
    bool HasPendingAsyncWork() const;
    // Usado por el builtin set_timeout para llevar la cuenta de timers
    // agendados-pero-no-disparados-ni-drenados todavia.
    void OnAsyncTimerScheduled();
    void OnAsyncTimerConsumed();

    // Fase 1 (async/await real). Task que resuelve solo (Nil) cuando el
    // PAL dispara su timer, via el mismo PostAsyncTask/SettleTask que usa
    // StartAsyncCall. Usado por el builtin delay(ms) (builtin_async.cpp)
    // para que `await delay(ms)` suspenda de verdad en vez de resolver en
    // el mismo tick.
    Value CreateTimerTask(avastd::uint32_t delay_ms);

    // Print sink used by the `print` builtin (builtin_natives.cpp). When
    // set, Print() forwards to the sink instead of writing straight to
    // the process's stdout -- this is what lets an embedder (e.g. Ava
    // Studio's Output console, see studio/src/engine/engine_bridge.cpp)
    // capture script output instead of it going to a stdout a GUI app
    // typically has no visible console for. Pass nullptr to restore the
    // stdout default. See ava_vm_set_print_callback in avalang.h.
    using PrintSink = avastd::function<void(const avastd::string&)>;
    void SetPrintSink(PrintSink sink);
    void Print(const avastd::string& text) const;

    using InputSink = avastd::function<avastd::string(const avastd::string& prompt)>;
    void SetInputSink(InputSink sink);
    avastd::string ReadLine(const avastd::string& prompt) const;

    // Fase 6 completion (08_DESIGNER_VIEW_PLAN.md Anexo 9.17's pendientes
    // 1-2, "ui.alert"/"ui.navigate" -- necesitan definir el mecanismo de
    // callback host<->VM, no solo la firma). Mismo patrón EXACTO que
    // PrintSink de arriba: un embedder (Ava Studio) instala un sink antes
    // de correr un script; sin sink, degrada a Print() con un prefijo, así
    // nunca se pierde silenciosamente lo que el script quiso comunicar aun
    // si el host no está escuchando ese evento en particular. Ver
    // ava_vm_set_alert_callback / ava_vm_set_navigate_callback en avalang.h,
    // y core/src/ui/builtins.cpp (ui.alert/ui.navigate) para el lado que
    // los dispara desde un script.
    using AlertSink = avastd::function<void(const avastd::string&)>;
    void SetAlertSink(AlertSink sink);
    void Alert(const avastd::string& message) const;

    using NavigateSink = avastd::function<void(const avastd::string&)>;
    void SetNavigateSink(NavigateSink sink);
    void Navigate(const avastd::string& route) const;

    // Fase 4 (avapack, ver plan_ava_pack.md): hooks opcionales alrededor de
    // la lectura de disco que hace DoImport (vm_import.cpp) para un modulo
    // importado. Mismo patron que PrintSink/AlertSink/NavigateSink de
    // arriba: sin hook instalado (nullptr, el default), DoImport se
    // comporta EXACTAMENTE igual que antes de este cambio -- ava_cli y
    // avahost normales nunca instalan esto y no se ven afectados.
    //
    // Pensado para runtime/avapack/src/main.cpp: en vez de volcar todo el
    // proyecto (ya descifrado) al temp dir de una sola vez (lo que hacia
    // la Fase 3), el .exe empacado deja el temp dir vacio y usa estos
    // hooks para materializar (descifrar + escribir) cada archivo de
    // modulo justo antes de que DoImport lo abra, y borrarlo apenas
    // termina de leerlo -- el archivo en claro nunca vive en disco mas
    // tiempo del que tarda ese std::ifstream en leerlo. Ver
    // ModuleFileHook resolved_path: la ruta ya resuelta (dentro del temp
    // dir) que DoImport esta por abrir/acaba de cerrar.
    using ModuleFileHook = avastd::function<void(const avastd::string& resolved_path)>;
    void SetBeforeModuleReadHook(ModuleFileHook hook);
    void SetAfterModuleReadHook(ModuleFileHook hook);
    const ModuleFileHook& GetBeforeModuleReadHook() const { return before_module_read_hook_; }
    const ModuleFileHook& GetAfterModuleReadHook() const { return after_module_read_hook_; }

    // Structured position of the most recent error caught by the C API
    // (ava_compile/ava_run/ava_call/ava_import, see public/src/c_api.cpp).
    // 1-based; 0 = unknown. Set from AvaError::line/column when the C API
    // catches one, or reset to 0 on any other kind of exception. Read via
    // ava_last_error_line/ava_last_error_column so an embedder (e.g. Ava
    // Studio) can highlight the offending line in an editor.
    int last_error_line = 0;
    int last_error_column = 0;
    // Path of the source file the most recent error came from (set from
    // AvaError::source alongside last_error_line/column above). Empty
    // when unknown, or when the error happened in the same file the
    // embedder already has open (e.g. a top-level ava_run failure) --
    // callers should fall back to the file they compiled/ran in that
    // case. Lets an embedder open the *right* file -- e.g. an error
    // raised inside an imported module -- rather than only a line
    // number in whichever file happens to be showing. Read via
    // ava_last_error_source.
    avastd::string last_error_source;

    // ExceptionHandler is defined in coroutine.h (needs to be usable from
    // Coroutine::exception_handlers before this class exists).

// Internal implementation friends - allow access to private members from vm_internal implementations
    friend void OpAdd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpSub(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpMul(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpDiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpIdiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpMod(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpPow(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNeg(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNot(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpInc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpDec(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpEq(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpEqK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNeK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpLt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpLe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpGt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpGe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpNewList(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpListAppend(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNewDict(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpGetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpSetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpNewClass(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpNewInstance(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpGetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpSetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpReturn(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpClosure(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpGetUpval(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpSetUpval(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend Value OpBaseCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpSlice(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpTry(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpTryEnd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpCatch(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpRaise(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpArgc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void OpYield(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpResume(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
    friend void OpAwait(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

    friend void HandleFrameError(VM& vm, size_t frame_idx, const avastd::exception& e);

private:
    Value ExecuteFrame(size_t frame_idx);
    // FASE 1 (async/await) fix: reanuda ejecucion desde el frame MAS
    // PROFUNDO de frames_ (no siempre frame 0), y cuando ese frame termina
    // normalmente (no suspendido), propaga su resultado hacia el frame
    // padre usando CallFrame::ret_slot y sigue ejecutando el padre desde
    // donde habia quedado -- en vez de reejecutar frame 0 desde su pc
    // (que ya avanzo de largo el CALL, perdiendose el resto de la cadena
    // de frames intermedios). Usado por resume()/RESUME para reanudar
    // corrutinas con yields anidados en funciones auxiliares.
    Value ResumeFromTop();

    // Fase 2 (async/await Task runtime). Calling an `async func` never runs
    // it inline like a normal call -- it starts a dedicated Coroutine for
    // the call (same suspension machinery as coroutine()/resume()) and
    // drives it up to its first `await` or its `return`, then hands back a
    // Task immediately either way. See vm_task.cpp.
    Value StartAsyncCall(const Value& closure_val, const avastd::vector<Value>& args);
    // Fase 3 (await en metodos de clase). Mismo mecanismo que
    // StartAsyncCall pero para un BoundMethod (obj.metodo_async(...)):
    // arma el entry_frame con bound->proto y bound->instance en el
    // registro 0 (this), en vez de asumir un Closure con proto propio.
    // Ver vm_call_op.cpp::OpCall, rama ValueType::Bound.
    //
    // base_lookup_class (bug #17): opcional, default nullptr. Cuando la
    // llamada viene de OpBaseCall (base.metodo_async(), no obj.metodo()
    // directo), hay que propagar la clase dueña del metodo que se esta
    // resolviendo al entry_frame de la coroutine recien creada -- mismo
    // mecanismo que el bug #14 ya resuelve para el camino sincrono
    // (CallFrame::base_lookup_class), necesario para que un SEGUNDO
    // base.xxx() dentro de este metodo async siga avanzando un nivel
    // real de la cadena de herencia en vez de repetir el mismo hop.
    Value StartAsyncBoundCall(const Value& bound_val, const avastd::vector<Value>& args,
                               ClassObj* base_lookup_class = nullptr);
    // Resumes a coroutine that's suspended on an `await`, either with the
    // awaited Task's result (is_error=false) or its error (is_error=true),
    // and settles this coroutine's own owner_task if it finishes as a
    // result (whether by returning normally or by an uncaught exception).
    void ResumeAwaitingCoroutine(Coroutine* co, Value value, bool is_error);
    // Marks `task` done with `value` (a result, or an error if is_error),
    // and schedules every coroutine that was awaiting it to resume via
    // PostAsyncTask, so settling never re-enters the VM synchronously from
    // inside another frame's execution.
    void SettleTask(TaskObj* task, Value value, bool is_error);

    // Upvalue "open"/"close" machinery (standard cell/box technique, same
    // shape as Lua's). An upvalue is "open" while its `location` still
    // points into the registers of the CallFrame that owns the captured
    // local -- reads/writes during that time go straight through
    // `location`, so every closure sharing that Upvalue (and the frame
    // itself) sees the same value. Returns the existing Upvalue for
    // `reg_idx` in `frame` if one is already open (so sibling closures
    // capturing the same local share one object), otherwise opens a new
    // one and records it in `frame.open_upvalues`.
    avastd::shared_ptr<Upvalue> FindOrCreateUpvalue(CallFrame& frame, avastd::uint32_t reg_idx);
    // Closes every upvalue `frame` opened: snapshots the live value into
    // Upvalue::value and repoints `location` at that snapshot, so
    // closures still holding the Upvalue keep working with self-contained
    // storage after `frame` (and its `registers` buffer) is destroyed.
    // Must be called on every CallFrame right before it's popped from
    // frames_ -- otherwise any Upvalue::location still pointing into its
    // registers becomes a dangling pointer.
    void CloseUpvalues(CallFrame& frame);
    // Used when `from`'s registers buffer is about to be destroyed but a
    // byte-identical copy of the frame already exists in `to` (e.g. `to`
    // is the entry `saved_frames_.back()[i]` that was just pushed as a
    // copy of `frames_[i]` before an async suspend clears `frames_`).
    // Unlike CloseUpvalues, this does NOT detach the upvalues into a
    // frozen snapshot: it repoints each open upvalue's `location` at the
    // corresponding register in `to` and transfers `open_upvalues`
    // ownership to `to`, so the upvalue stays "open" and any closure
    // holding it keeps reading/writing through live register storage --
    // just `to`'s buffer instead of `from`'s. This is what lets a
    // captured local kept alive across a suspended `await`/`yield` still
    // be shared correctly with the resumed frame once `to` eventually
    // replaces `frames_` again.
    void RelocateUpvalues(CallFrame& from, CallFrame& to);

    PrintSink print_sink_;
    InputSink input_sink_;
    AlertSink alert_sink_;
    NavigateSink navigate_sink_;
    ModuleFileHook before_module_read_hook_;
    ModuleFileHook after_module_read_hook_;
    avastd::string current_dir_;
    avastd::unordered_map<avastd::string, Value> globals_;
    avastd::vector<CallFrame> frames_;
    ModuleResolver module_resolver_;
    ModuleCache module_cache_;
    avastd::string current_module_;
    avastd::unordered_map<avastd::string, avastd::pair<AvaNativeFn, void*>> builtin_methods_;
    // Phase 1 of AVALANG_IMPORT_SYSTEM_PLAN.md. Keyed by exact
    // module_path (e.g. "system"), checked in DoImport before any file
    // resolution is attempted.
    avastd::unordered_map<avastd::string, NativeModuleFactory> native_modules_;

    Value pending_exception_;
    bool try_had_exception_ = false;
    avastd::vector<ExceptionHandler> exception_handlers_;
    // Stack of saved outer exception-handler stacks, pushed/popped in
    // lockstep with saved_frames_ below. exception_handlers_ entries store
    // a frame_idx that's only meaningful relative to the frames_ vector
    // that was active when OpTry registered them. Nested async/coroutine
    // runs clear+reuse frames_ from index 0, so without this an outer
    // try/catch's handler can collide with an unrelated inner call's
    // frame_idx and wrongly swallow/misroute its exception (see OpTry).
    avastd::vector<avastd::vector<ExceptionHandler>> saved_exception_handlers_;

    avastd::vector<Coroutine*> coroutine_resumers_;
    avastd::vector<Coroutine*> created_coroutines_;
    avastd::vector<TaskObj*> created_tasks_;
    Value yielded_values_;
    // Stack of saved outer frame stacks, one entry per nested
    // coroutine/task run currently in progress (StartAsyncCall,
    // ResumeAwaitingCoroutine, OpResume, VM::Call's Coroutine branch all
    // push/pop here). Must be a stack, not a single slot: an async func
    // calling another async func synchronously (no suspension in between)
    // re-enters one of these while the outer one hasn't restored yet, and
    // a single slot would get clobbered by the inner call.
    avastd::vector<avastd::vector<CallFrame>> saved_frames_;
    bool is_coroutine_suspended_ = false;
    Coroutine* current_coroutine_ = nullptr;

    // Fase 5 (Async Runtime): ver comentarios de PostAsyncTask/PumpAsyncEvents.
    avastd::mutex async_mutex_;
    avastd::vector<avastd::function<void()>> async_ready_queue_;
    avastd::atomic<int> async_pending_timers_{0};
};

} // namespace ava

#endif // AVA_VM_VM_H