#ifndef AVA_VM_VM_H
#define AVA_VM_VM_H

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

private:
    Value ExecuteFrame(size_t frame_idx);

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