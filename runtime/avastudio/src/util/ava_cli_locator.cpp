#include "util/ava_cli_locator.h"

#include <system_error>
#include <vector>

#if defined(_WIN32)
    #define AVASTUDIO_EXE_SUFFIX ".exe"
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #define AVASTUDIO_EXE_SUFFIX ""
    #include <unistd.h>
    #include <limits.h>
#endif

namespace studio {

namespace fs = std::filesystem;

fs::path SelfExecutableDir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    std::error_code ec;
    fs::path dir = fs::path(std::string(buf, len)).parent_path();
    return fs::exists(dir, ec) ? dir : fs::path{};
#else
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return {};
    return fs::path(std::string(buf, static_cast<size_t>(len))).parent_path();
#endif
}

fs::path DetectAvaCliPath() {
    const std::string exe_name = std::string("ava_cli") + AVASTUDIO_EXE_SUFFIX;
    fs::path self_dir = SelfExecutableDir();
    if (self_dir.empty()) return {};

    std::vector<fs::path> candidates = {
        self_dir / exe_name,
        self_dir.parent_path().parent_path() / "avalang" / self_dir.filename() / exe_name,
        self_dir.parent_path().parent_path() / "avalang" / exe_name,
        self_dir.parent_path() / "avalang" / exe_name,
    };
    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate;
    }
    return {};
}

}
