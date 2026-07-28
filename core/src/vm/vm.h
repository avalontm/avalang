#ifndef AVA_VM_VM_H
#define AVA_VM_VM_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>

#include "value.h"
#include "proto.h"
#include "closure.h"
#include "module.h"
#include "coroutine.h"
#include "vm_helpers.h"
#include "../../../public/include/avalang.h"

namespace ava {

class VM {
public:
    VM();
    ~VM();

    void RegisterNative(const std::string& name, AvaNativeFn fn, void* user_data);
    void RegisterBuiltinMethod(const std::string& name, AvaNativeFn fn, void* user_data);

    Value GetGlobal(const std::string& name) const;
    void  SetGlobal(const std::string& name, Value value);

    Value Run(const std::shared_ptr<Proto>& main);
    Value RunFile(const std::string& file_path);

    Value Call(const Value& callable, const std::vector<Value>& args);
    
    bool HasBuiltinMethod(const std::string& name) const;
    Value GetBuiltinMethod(const std::string& name) const;

    std::unordered_map<std::string, Value>& Globals() { return globals_; }

    ModuleResolver& GetModuleResolver() { return module_resolver_; }
    ModuleCache& GetModuleCache() { return module_cache_; }

    std::string GetCurrentDir() const;
    void SetCurrentDir(const std::string& dir);
    Value DoImport(const std::string& module_path, const std::string& alias);

    void RaiseException(const Value& exc);
    Value GetAndClearException();
    bool HasException() const;

    Coroutine* CreateCoroutine(const Value& func);

    // Print sink used by the `print` builtin (builtin_natives.cpp). When
    // set, Print() forwards to the sink instead of writing straight to
    // the process's stdout -- this is what lets an embedder (e.g. Ava
    // Studio's Output console, see studio/src/engine/engine_bridge.cpp)
    // capture script output instead of it going to a stdout a GUI app
    // typically has no visible console for. Pass nullptr to restore the
    // stdout default. See ava_vm_set_print_callback in avalang.h.
    using PrintSink = std::function<void(const std::string&)>;
    void SetPrintSink(PrintSink sink);
    void Print(const std::string& text) const;

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
    using AlertSink = std::function<void(const std::string&)>;
    void SetAlertSink(AlertSink sink);
    void Alert(const std::string& message) const;

    using NavigateSink = std::function<void(const std::string&)>;
    void SetNavigateSink(NavigateSink sink);
    void Navigate(const std::string& route) const;

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
    std::string last_error_source;

    struct ExceptionHandler {
        size_t catch_pc;
    };

// Internal implementation friends - allow access to private members from vm_internal implementations
    friend void OpAdd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpSub(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpMul(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpDiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpIdiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpMod(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpPow(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNeg(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNot(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpInc(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpDec(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpEq(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpEqK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNeK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpLt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpLe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpGt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpGe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpNewList(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpListAppend(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNewDict(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpGetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpSetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpNewClass(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpNewInstance(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpGetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpSetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpReturn(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpClosure(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpGetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpSetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend Value OpBaseCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpSlice(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpTry(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpTryEnd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpCatch(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpRaise(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpArgc(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void OpYield(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
    friend void OpResume(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

    friend void HandleFrameError(VM& vm, size_t frame_idx, const std::exception& e);

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

    PrintSink print_sink_;
    AlertSink alert_sink_;
    NavigateSink navigate_sink_;
    std::string current_dir_;
    std::unordered_map<std::string, Value> globals_;
    std::vector<CallFrame> frames_;
    ModuleResolver module_resolver_;
    ModuleCache module_cache_;
    std::string current_module_;
    std::unordered_map<std::string, std::pair<AvaNativeFn, void*>> builtin_methods_;

    Value pending_exception_;
    bool try_had_exception_ = false;
    std::vector<ExceptionHandler> exception_handlers_;

    std::vector<Coroutine*> coroutine_resumers_;
    std::vector<Coroutine*> created_coroutines_;
    Value yielded_values_;
    std::vector<CallFrame> saved_frames_;
    bool is_coroutine_suspended_ = false;
    Coroutine* current_coroutine_ = nullptr;
};

} // namespace ava

#endif // AVA_VM_VM_H