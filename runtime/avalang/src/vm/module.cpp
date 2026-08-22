#include "module.h"
#include "vm_platform_accessor.h"

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#else
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#endif

namespace ava {

static avastd::string JoinPath(const avastd::string& a, const avastd::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = PATH_SEPARATOR_CHAR;
    if (a.back() == sep || a.back() == '/') return a + b;
    return a + PATH_SEPARATOR + b;
}

static bool FileExists(const avastd::string& path) {
    return VmPlatformAccessor::Get().FileSystem().Exists(path);
}

ModuleResolver::ModuleResolver() {
    avastd::string cwd = VmPlatformAccessor::Get().Environment().GetCurrentDirectory();
    AddSearchPath(cwd);
}

void ModuleResolver::AddSearchPath(const avastd::string& path) {
    for (const auto& p : search_paths_) {
        if (p == path) return;
    }
    search_paths_.push_back(path);
}

void ModuleResolver::SetStdlibPath(const avastd::string& path) {
    stdlib_path_ = path;
}

void ModuleResolver::SetReloadMode(bool reload) {
    reload_mode_ = reload;
}

avastd::string ModuleResolver::PathToFilePath(const avastd::string& module_path) {
    avastd::string result = module_path;
    for (char& c : result) {
        if (c == '.') c = PATH_SEPARATOR_CHAR;
    }
    return result;
}

avastd::string ModuleResolver::ResolveModulePath(const avastd::string& module_path, const avastd::string& current_dir) {
    avastd::string file_path = PathToFilePath(module_path);
    
    avastd::string direct_path = JoinPath(current_dir, file_path + ".ava");
    if (FileExists(direct_path)) return direct_path;
    
    avastd::string index_path = JoinPath(current_dir, file_path + PATH_SEPARATOR + "index.ava");
    if (FileExists(index_path)) return index_path;
    
    for (const auto& search_path : search_paths_) {
        avastd::string p = JoinPath(search_path, file_path + ".ava");
        if (FileExists(p)) return p;
        
        p = JoinPath(search_path, file_path + PATH_SEPARATOR + "index.ava");
        if (FileExists(p)) return p;
    }
    
    if (!stdlib_path_.empty()) {
        avastd::string p = JoinPath(stdlib_path_, file_path + ".ava");
        if (FileExists(p)) return p;
        
        p = JoinPath(stdlib_path_, file_path + PATH_SEPARATOR + "index.ava");
        if (FileExists(p)) return p;
    }
    
    return "";
}

bool ModuleResolver::ModuleExists(const avastd::string& module_path, const avastd::string& current_dir) {
    return !ResolveModulePath(module_path, current_dir).empty();
}

void ModuleCache::Add(const avastd::string& module_name, avastd::shared_ptr<Proto> proto, const avastd::string& file_path) {
    // `proto` llega por valor (el caller ya entregó su propia copia/
    // ownership) -- moverlo adentro en vez de copiarlo evita un
    // incremento+decremento atomico redundante del control block de
    // shared_ptr en cada import de modulo (Fase 3, "Reducir std::shared_ptr").
    modules_[module_name] = avastd::move(proto);
    file_paths_[module_name] = file_path;
}

avastd::shared_ptr<Proto> ModuleCache::Get(const avastd::string& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) return it->second;
    return nullptr;
}

bool ModuleCache::Exists(const avastd::string& module_name) {
    return modules_.find(module_name) != modules_.end();
}

void ModuleCache::Remove(const avastd::string& module_name) {
    modules_.erase(module_name);
    file_paths_.erase(module_name);
}

void ModuleCache::Clear() {
    modules_.clear();
    file_paths_.clear();
    loading_modules_.clear();
}

void ModuleCache::BeginLoading(const avastd::string& module_name) {
    loading_modules_.insert(module_name);
}

bool ModuleCache::IsLoading(const avastd::string& module_name) {
    return loading_modules_.find(module_name) != loading_modules_.end();
}

void ModuleCache::EndLoading(const avastd::string& module_name) {
    loading_modules_.erase(module_name);
}

avastd::string ModuleCache::GetFilePath(const avastd::string& module_name) {
    auto it = file_paths_.find(module_name);
    if (it != file_paths_.end()) return it->second;
    return "";
}

} // namespace ava