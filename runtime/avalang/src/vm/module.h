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

    bool IsModulePathADirectory(const avastd::string& module_path, const avastd::string& current_dir);
    
    avastd::string GetStdlibPath() const { return stdlib_path_; }
    const avastd::vector<avastd::string>& GetSearchPaths() const { return search_paths_; }
    static avastd::vector<avastd::string> ListLooseAvaFiles(const avastd::string& dir);

private:
    avastd::string stdlib_path_;
    avastd::vector<avastd::string> search_paths_;
    bool reload_mode_ = false;
    
    avastd::string PathToFilePath(const avastd::string& module_path);
};

// Mirrors ModuleResolver's search_paths_ (e.g. the project/script root avacli
// and AvaStudio register via AddSearchPath) in a process-wide list that the
// compiler's compile-time sibling-import class harvesting can also consult.
// That harvesting (compiler.cpp: ResolveSiblingImportFiles) runs standalone,
// without access to any particular VM's ModuleResolver instance, and used to
// only fall back to the OS current working directory -- which is often not
// the project root (e.g. running `ava_cli path/to/project/main.ava` from a
// different directory), so a file whose imports reference a sibling folder
// (e.g. "import Interfaces.IAnimal" from inside Models/Dog.ava, where
// Interfaces/ sits next to Models/, not inside it) would silently fail to
// resolve at compile time even though the real runtime import (which does go
// through ModuleResolver) succeeded.
class AVA_MODULE_API GlobalSearchRoots {
public:
    static void Add(const avastd::string& path);
    static const avastd::vector<avastd::string>& Get();
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
    bool HasModuleValue(const avastd::string& module_name);
    Value GetModuleValue(const avastd::string& module_name);
    void SetModuleValue(const avastd::string& module_name, Value module_dict);

private:
    avastd::unordered_map<avastd::string, avastd::shared_ptr<Proto>> modules_;
    avastd::unordered_map<avastd::string, avastd::string> file_paths_;
    avastd::unordered_set<avastd::string> loading_modules_;
    avastd::unordered_map<avastd::string, Value> module_values_;
};

} // namespace ava

#endif // AVA_VM_MODULE_H