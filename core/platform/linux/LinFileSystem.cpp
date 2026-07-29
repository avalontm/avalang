#include "LinFileSystem.h"

// STUB implementation. Every method is a placeholder that fails cleanly
// instead of touching real POSIX file APIs (open/read/write/opendir/...).
// TODO(Phase 5): implement against POSIX (Linux) / Darwin (macOS) APIs,
// mirroring core/platform/windows/LinFileSystem.cpp.

namespace ava {
namespace platform {
namespace linux_ {

bool LinFileSystem::ReadFile(const std::string& /*path*/, std::string& /*out_content*/) {
    return false;
}

bool LinFileSystem::WriteFile(const std::string& /*path*/, const std::string& /*content*/) {
    return false;
}

bool LinFileSystem::DeleteFile(const std::string& /*path*/) {
    return false;
}

bool LinFileSystem::CreateDirectory(const std::string& /*path*/) {
    return false;
}

bool LinFileSystem::DeleteDirectory(const std::string& /*path*/) {
    return false;
}

bool LinFileSystem::EnumerateDirectory(const std::string& /*path*/, std::vector<DirEntry>& out_entries) {
    out_entries.clear();
    return false;
}

bool LinFileSystem::Exists(const std::string& /*path*/) {
    return false;
}

bool LinFileSystem::IsDirectory(const std::string& /*path*/) {
    return false;
}

int64_t LinFileSystem::FileSize(const std::string& /*path*/) {
    return -1;
}

std::string LinFileSystem::GetExecutableDirectory() {
    return std::string();
}

} // namespace linux_
} // namespace platform
} // namespace ava
