#include "util/data_dir.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace studio::util {

namespace {
namespace fs = std::filesystem;
}

std::string ResolveDataDir() {
    fs::path base = fs::current_path();
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        base = fs::path(exe_path).parent_path();
    }
#endif
    fs::path data = base / "data";
    std::string result = data.string();
    if (result.empty() || (result.back() != '/' && result.back() != '\\')) {
        result += "/";
    }
    return result;
}

std::string ResolveDefaultModulesDir() {
    fs::path base = fs::current_path();
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        base = fs::path(exe_path).parent_path();
    }
#endif
    return (base / "modules").string();
}

bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

} // namespace studio::util
