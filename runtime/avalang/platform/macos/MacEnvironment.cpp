#include "MacEnvironment.h"

#include <cstdlib>
#include <unistd.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ava {
namespace platform {
namespace macos_ {

namespace {

std::vector<std::string> g_cmdline_args;

} // namespace

bool MacEnvironment::GetEnvVar(const std::string& name, std::string& out_value) {
    const char* v = std::getenv(name.c_str());
    if (!v) return false;
    out_value = v;
    return true;
}

bool MacEnvironment::SetEnvVar(const std::string& name, const std::string& value) {
    return ::setenv(name.c_str(), value.c_str(), 1) == 0;
}

std::string MacEnvironment::GetCurrentDirectory() {
    char buf[PATH_MAX];
    if (!::getcwd(buf, sizeof(buf))) return std::string();
    return std::string(buf);
}

bool MacEnvironment::SetCurrentDirectory(const std::string& path) {
    return ::chdir(path.c_str()) == 0;
}

std::vector<std::string> MacEnvironment::GetCommandLineArgs() {
    return g_cmdline_args;
}

void SetCommandLineArgs(int argc, char** argv) {
    g_cmdline_args.clear();
    for (int i = 0; i < argc; ++i) {
        g_cmdline_args.push_back(argv[i]);
    }
}

} // namespace macos_
} // namespace platform
} // namespace ava
