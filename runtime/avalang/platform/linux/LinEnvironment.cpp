#include "LinEnvironment.h"

#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ava {
namespace platform {
namespace linux_ {

namespace {

std::vector<std::string> g_cmdline_args;

} // namespace

bool LinEnvironment::GetEnvVar(const std::string& name, std::string& out_value) {
    const char* v = std::getenv(name.c_str());
    if (!v) return false;
    out_value = v;
    return true;
}

bool LinEnvironment::SetEnvVar(const std::string& name, const std::string& value) {
    return ::setenv(name.c_str(), value.c_str(), 1) == 0;
}

std::string LinEnvironment::GetCurrentDirectory() {
    char buf[PATH_MAX];
    if (!::getcwd(buf, sizeof(buf))) return std::string();
    return std::string(buf);
}

bool LinEnvironment::SetCurrentDirectory(const std::string& path) {
    return ::chdir(path.c_str()) == 0;
}

std::vector<std::string> LinEnvironment::GetCommandLineArgs() {
    return g_cmdline_args;
}

// Called once from main() on Linux to seed the command-line args captured
// at process start (read from /proc/self/cmdline). Ava CLI sets this via
// a public entry point; if never called, returns an empty vector (which
// is the same behavior as the Windows version when CommandLineToArgvW
// is unavailable).
void SetCommandLineArgs(int argc, char** argv) {
    g_cmdline_args.clear();
    for (int i = 0; i < argc; ++i) {
        g_cmdline_args.push_back(argv[i]);
    }
}

} // namespace linux_
} // namespace platform
} // namespace ava
