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

#ifdef _WIN32
  #define AVA_API __declspec(dllexport)
#else
  #define AVA_API __attribute__((visibility("default")))
#endif

namespace ava {

struct CallFrame {
    std::shared_ptr<Proto> proto;
    std::shared_ptr<Closure> closure;
    std::vector<Value> registers;
    uint32_t pc = 0;
    std::string module_dir;
};

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

private:
    Value ExecuteFrame(size_t frame_idx);

    std::string current_dir_;
    std::unordered_map<std::string, Value> globals_;
    std::vector<CallFrame> frames_;
    ModuleResolver module_resolver_;
    ModuleCache module_cache_;
    std::string current_module_;
    std::unordered_map<std::string, std::pair<AvaNativeFn, void*>> builtin_methods_;
};

} // namespace ava

#endif // AVA_VM_VM_H