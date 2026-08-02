#include "WinEnvironment.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h> // CommandLineToArgvW -- link Shell32.lib

// Windows.h #defines GetCurrentDirectory/SetCurrentDirectory as ANSI/Unicode
// dispatch macros (-> ...A here), which would otherwise rewrite the
// out-of-line definitions below to names the header never declared.
// Undefine them; the explicit *A calls inside this file already spell out
// the ANSI entry points directly.
#undef GetCurrentDirectory
#undef SetCurrentDirectory

namespace ava {
namespace platform {
namespace windows {

namespace {

std::string WideToUtf8(const wchar_t* wide) {
    if (wide == nullptr) {
        return {};
    }
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size_needed) - 1, '\0'); // exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], size_needed, nullptr, nullptr);
    return result;
}

} // namespace

bool WinEnvironment::GetEnvVar(const std::string& name, std::string& out_value) {
    char buffer[32768]; // Windows env var value hard limit
    DWORD len = GetEnvironmentVariableA(name.c_str(), buffer, static_cast<DWORD>(sizeof(buffer)));
    if (len == 0) {
        return false;
    }
    out_value.assign(buffer, len);
    return true;
}

bool WinEnvironment::SetEnvVar(const std::string& name, const std::string& value) {
    return SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0;
}

std::string WinEnvironment::GetCurrentDirectory() {
    char buffer[MAX_PATH];
    DWORD len = ::GetCurrentDirectoryA(static_cast<DWORD>(sizeof(buffer)), buffer);
    if (len == 0) {
        return {};
    }
    return std::string(buffer, len);
}

bool WinEnvironment::SetCurrentDirectory(const std::string& path) {
    return ::SetCurrentDirectoryA(path.c_str()) != 0;
}

std::vector<std::string> WinEnvironment::GetCommandLineArgs() {
    std::vector<std::string> args;

    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv_w == nullptr) {
        return args;
    }

    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.push_back(WideToUtf8(argv_w[i]));
    }

    LocalFree(argv_w);
    return args;
}

} // namespace windows
} // namespace platform
} // namespace ava
