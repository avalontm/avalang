#ifndef AVA_VM_MODULE_H
#define AVA_VM_MODULE_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include "proto.h"

#ifdef _WIN32
  #define AVA_MODULE_API __declspec(dllexport)
#else
  #define AVA_MODULE_API __attribute__((visibility("default")))
#endif

namespace ava {

struct Module {
    std::shared_ptr<Proto> proto;
    std::string file_path;
    std::string source_name;
};

class AVA_MODULE_API ModuleResolver {
public:
    ModuleResolver();
    
    void AddSearchPath(const std::string& path);
    void SetStdlibPath(const std::string& path);
    void SetReloadMode(bool reload);
    
    std::string ResolveModulePath(const std::string& module_path, const std::string& current_dir);
    
    bool ModuleExists(const std::string& module_path, const std::string& current_dir);
    
    std::string GetStdlibPath() const { return stdlib_path_; }
    const std::vector<std::string>& GetSearchPaths() const { return search_paths_; }

private:
    std::string stdlib_path_;
    std::vector<std::string> search_paths_;
    bool reload_mode_ = false;
    
    std::string PathToFilePath(const std::string& module_path);
};

class AVA_MODULE_API ModuleCache {
public:
    void Add(const std::string& module_name, std::shared_ptr<Proto> proto, const std::string& file_path);
    std::shared_ptr<Proto> Get(const std::string& module_name);
    bool Exists(const std::string& module_name);
    void Remove(const std::string& module_name);
    void Clear();
    
    void BeginLoading(const std::string& module_name);
    bool IsLoading(const std::string& module_name);
    void EndLoading(const std::string& module_name);
    
    std::string GetFilePath(const std::string& module_name);

private:
    std::unordered_map<std::string, std::shared_ptr<Proto>> modules_;
    std::unordered_map<std::string, std::string> file_paths_;
    std::unordered_set<std::string> loading_modules_;
};

} // namespace ava

#endif // AVA_VM_MODULE_H