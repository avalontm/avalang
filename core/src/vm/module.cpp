#include "module.h"
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define GetCurrentDir _getcwd
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#else
#include <unistd.h>
#include <sys/stat.h>
#define GetCurrentDir getcwd
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#endif

namespace ava {

static std::string GetCurrentWorkingDir() {
    char buff[4096];
    GetCurrentDir(buff, sizeof(buff));
    return std::string(buff);
}

static std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = PATH_SEPARATOR_CHAR;
    if (a.back() == sep || a.back() == '/') return a + b;
    return a + PATH_SEPARATOR + b;
}

#ifdef _WIN32
static bool FileExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES);
}
#else
static bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
#endif

static std::string GetFileDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

ModuleResolver::ModuleResolver() {
    std::string cwd = GetCurrentWorkingDir();
    AddSearchPath(cwd);
}

void ModuleResolver::AddSearchPath(const std::string& path) {
    for (const auto& p : search_paths_) {
        if (p == path) return;
    }
    search_paths_.push_back(path);
}

void ModuleResolver::SetStdlibPath(const std::string& path) {
    stdlib_path_ = path;
}

void ModuleResolver::SetReloadMode(bool reload) {
    reload_mode_ = reload;
}

std::string ModuleResolver::PathToFilePath(const std::string& module_path) {
    std::string result = module_path;
    for (char& c : result) {
        if (c == '.') c = PATH_SEPARATOR_CHAR;
    }
    return result;
}

std::string ModuleResolver::ResolveModulePath(const std::string& module_path, const std::string& current_dir) {
    std::string file_path = PathToFilePath(module_path);
    
    std::string direct_path = JoinPath(current_dir, file_path + ".ava");
    if (FileExists(direct_path)) return direct_path;
    
    std::string index_path = JoinPath(current_dir, file_path + PATH_SEPARATOR + "index.ava");
    if (FileExists(index_path)) return index_path;
    
    for (const auto& search_path : search_paths_) {
        std::string p = JoinPath(search_path, file_path + ".ava");
        if (FileExists(p)) return p;
        
        p = JoinPath(search_path, file_path + PATH_SEPARATOR + "index.ava");
        if (FileExists(p)) return p;
    }
    
    if (!stdlib_path_.empty()) {
        std::string p = JoinPath(stdlib_path_, file_path + ".ava");
        if (FileExists(p)) return p;
        
        p = JoinPath(stdlib_path_, file_path + PATH_SEPARATOR + "index.ava");
        if (FileExists(p)) return p;
    }
    
    return "";
}

bool ModuleResolver::ModuleExists(const std::string& module_path, const std::string& current_dir) {
    return !ResolveModulePath(module_path, current_dir).empty();
}

void ModuleCache::Add(const std::string& module_name, std::shared_ptr<Proto> proto, const std::string& file_path) {
    modules_[module_name] = proto;
    file_paths_[module_name] = file_path;
}

std::shared_ptr<Proto> ModuleCache::Get(const std::string& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) return it->second;
    return nullptr;
}

bool ModuleCache::Exists(const std::string& module_name) {
    return modules_.find(module_name) != modules_.end();
}

void ModuleCache::Remove(const std::string& module_name) {
    modules_.erase(module_name);
    file_paths_.erase(module_name);
}

void ModuleCache::Clear() {
    modules_.clear();
    file_paths_.clear();
    loading_modules_.clear();
}

void ModuleCache::BeginLoading(const std::string& module_name) {
    loading_modules_.insert(module_name);
}

bool ModuleCache::IsLoading(const std::string& module_name) {
    return loading_modules_.find(module_name) != loading_modules_.end();
}

void ModuleCache::EndLoading(const std::string& module_name) {
    loading_modules_.erase(module_name);
}

std::string ModuleCache::GetFilePath(const std::string& module_name) {
    auto it = file_paths_.find(module_name);
    if (it != file_paths_.end()) return it->second;
    return "";
}

} // namespace ava