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

avastd::vector<avastd::string> ModuleResolver::ListLooseAvaFiles(const avastd::string& dir) {
    avastd::vector<platform::DirEntry> entries;
    avastd::vector<avastd::string> result;
    if (!VmPlatformAccessor::Get().FileSystem().EnumerateDirectory(dir, entries)) return result;
    for (auto& entry : entries) {
        if (entry.is_directory) continue;
        const avastd::string& name = entry.name;
        if (name.size() <= 4) continue;
        if (name.compare(name.size() - 4, 4, ".ava") != 0) continue;
        result.push_back(JoinPath(dir, name));
    }
    avastd::sort(result.begin(), result.end());
    return result;
}

ModuleResolver::ModuleResolver() {
    platform::IPlatform& plat = VmPlatformAccessor::Get();
    platform::IEnvironment& env = plat.Environment();
    avastd::string cwd = env.GetCurrentDirectory();
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

    auto try_base = [&](const avastd::string& base) -> avastd::string {
        avastd::string direct_path = JoinPath(base, file_path + ".ava");
        if (FileExists(direct_path)) return direct_path;

        avastd::string module_dir = JoinPath(base, file_path);
        avastd::string index_path = JoinPath(module_dir, "index.ava");
        if (FileExists(index_path)) return index_path;

        if (VmPlatformAccessor::Get().FileSystem().IsDirectory(module_dir) &&
            !ListLooseAvaFiles(module_dir).empty()) {
            return module_dir;
        }

        return avastd::string();
    };

    avastd::string found = try_base(current_dir);
    if (!found.empty()) return found;

    for (const auto& search_path : search_paths_) {
        found = try_base(search_path);
        if (!found.empty()) return found;
    }

    if (!stdlib_path_.empty()) {
        found = try_base(stdlib_path_);
        if (!found.empty()) return found;
    }

    return "";
}

bool ModuleResolver::ModuleExists(const avastd::string& module_path, const avastd::string& current_dir) {
    return !ResolveModulePath(module_path, current_dir).empty();
}

void ModuleCache::Add(const avastd::string& module_name, avastd::shared_ptr<Proto> proto, const avastd::string& file_path) {
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
    module_values_.erase(module_name);
}

void ModuleCache::Clear() {
    modules_.clear();
    file_paths_.clear();
    loading_modules_.clear();
    module_values_.clear();
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

bool ModuleCache::HasModuleValue(const avastd::string& module_name) {
    return module_values_.find(module_name) != module_values_.end();
}

Value ModuleCache::GetModuleValue(const avastd::string& module_name) {
    auto it = module_values_.find(module_name);
    if (it != module_values_.end()) return it->second;
    return Value::Nil();
}

void ModuleCache::SetModuleValue(const avastd::string& module_name, Value module_dict) {
    module_values_[module_name] = avastd::move(module_dict);
}

} // namespace ava