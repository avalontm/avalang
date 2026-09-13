#include "util/data_dir.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

namespace studio::util {

namespace {
namespace fs = std::filesystem;

fs::path ExecutableDir() {
    std::error_code ec;
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return fs::path(exe_path).parent_path();
#elif defined(__APPLE__)
    char exe_path[PATH_MAX];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        fs::path resolved = fs::canonical(fs::path(exe_path), ec);
        if (!ec) return resolved.parent_path();
        return fs::path(exe_path).parent_path();
    }
#elif defined(__linux__)
    fs::path resolved = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return resolved.parent_path();
#endif
    return fs::current_path(ec);
}

}

std::string ResolveDataDir() {
    fs::path data = ExecutableDir() / "data";
    std::string result = data.string();
    if (result.empty() || (result.back() != '/' && result.back() != '\\')) {
        result += "/";
    }
    return result;
}

std::string ResolveDefaultModulesDir() {
    return (ExecutableDir() / "modules").string();
}

bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

}
