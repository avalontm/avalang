#ifndef AVA_VM_VM_H
#define AVA_VM_VM_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "value.h"
#include "proto.h"
#include "closure.h"
#include "module.h"
#include "coroutine.h"

#ifdef _WIN32
  #define AVA_API __declspec(dllexport)
#else
  #define AVA_API __attribute__((visibility("default")))
#endif

namespace ava {

class AVA_API VM {
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

private:
    Value ExecuteFrame(size_t frame_idx);

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
    struct ExceptionHandler {
        size_t catch_pc;
    };
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