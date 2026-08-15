#include "LinFileSystem.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ava {
namespace platform {
namespace linux_ {

namespace {

bool StatPath(const std::string& path, struct stat& st) {
    return ::stat(path.c_str(), &st) == 0;
}

} // namespace

bool LinFileSystem::ReadFile(const std::string& path, std::string& out_content) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out_content = ss.str();
    return true;
}

bool LinFileSystem::WriteFile(const std::string& path, const std::string& content) {
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), content.size());
    return f.good();
}

bool LinFileSystem::DeleteFile(const std::string& path) {
    return ::unlink(path.c_str()) == 0;
}

bool LinFileSystem::CreateDirectory(const std::string& path) {
    return ::mkdir(path.c_str(), 0755) == 0;
}

bool LinFileSystem::DeleteDirectory(const std::string& path) {
    return ::rmdir(path.c_str()) == 0;
}

bool LinFileSystem::EnumerateDirectory(const std::string& path, std::vector<DirEntry>& out_entries) {
    out_entries.clear();
    DIR* dir = ::opendir(path.c_str());
    if (!dir) return false;
    while (struct dirent* ent = ::readdir(dir)) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        DirEntry e;
        e.name = name;
        std::string full = path + "/" + name;
        struct stat st;
        if (StatPath(full, st)) {
            e.is_directory = S_ISDIR(st.st_mode);
        }
        out_entries.push_back(e);
    }
    ::closedir(dir);
    return true;
}

bool LinFileSystem::Exists(const std::string& path) {
    struct stat st;
    return StatPath(path, st);
}

bool LinFileSystem::IsDirectory(const std::string& path) {
    struct stat st;
    if (!StatPath(path, st)) return false;
    return S_ISDIR(st.st_mode);
}

int64_t LinFileSystem::FileSize(const std::string& path) {
    struct stat st;
    if (!StatPath(path, st)) return -1;
    return static_cast<int64_t>(st.st_size);
}

std::string LinFileSystem::GetExecutableDirectory() {
    char buf[PATH_MAX];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    std::string exe = buf;
    size_t slash = exe.find_last_of('/');
    if (slash == std::string::npos) return std::string();
    return exe.substr(0, slash);
}

} // namespace linux_
} // namespace platform
} // namespace ava
