#ifndef AVA_VM_MODULE_H
#define AVA_VM_MODULE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "proto.h"

#ifdef _WIN32
  #define AVA_MODULE_API __declspec(dllexport)
#else
  #define AVA_MODULE_API __attribute__((visibility("default")))
#endif

namespace ava {

struct Module {
    avastd::shared_ptr<Proto> proto;
    avastd::string file_path;
    avastd::string source_name;
};

class AVA_MODULE_API ModuleResolver {
public:
    ModuleResolver();
    
    void AddSearchPath(const avastd::string& path);
    void SetStdlibPath(const avastd::string& path);
    void SetReloadMode(bool reload);
    
    avastd::string ResolveModulePath(const avastd::string& module_path, const avastd::string& current_dir);
    
    bool ModuleExists(const avastd::string& module_path, const avastd::string& current_dir);
    
    avastd::string GetStdlibPath() const { return stdlib_path_; }
    const avastd::vector<avastd::string>& GetSearchPaths() const { return search_paths_; }

private:
    avastd::string stdlib_path_;
    avastd::vector<avastd::string> search_paths_;
    bool reload_mode_ = false;
    
    avastd::string PathToFilePath(const avastd::string& module_path);
};

class AVA_MODULE_API ModuleCache {
public:
    void Add(const avastd::string& module_name, avastd::shared_ptr<Proto> proto, const avastd::string& file_path);
    avastd::shared_ptr<Proto> Get(const avastd::string& module_name);
    bool Exists(const avastd::string& module_name);
    void Remove(const avastd::string& module_name);
    void Clear();
    
    void BeginLoading(const avastd::string& module_name);
    bool IsLoading(const avastd::string& module_name);
    void EndLoading(const avastd::string& module_name);
    
    avastd::string GetFilePath(const avastd::string& module_name);

private:
    avastd::unordered_map<avastd::string, avastd::shared_ptr<Proto>> modules_;
    avastd::unordered_map<avastd::string, avastd::string> file_paths_;
    avastd::unordered_set<avastd::string> loading_modules_;
};

} // namespace ava

#endif // AVA_VM_MODULE_H