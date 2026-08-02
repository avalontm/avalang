#include "MacFileSystem.h"

// STUB implementation. Every method is a placeholder that fails cleanly
// instead of touching real POSIX file APIs (open/read/write/opendir/...).
// TODO(Phase 6): implement against POSIX (Linux) / Darwin (macOS) APIs,
// mirroring core/platform/windows/MacFileSystem.cpp.

namespace ava {
namespace platform {
namespace macos_ {

bool MacFileSystem::ReadFile(const std::string& /*path*/, std::string& /*out_content*/) {
    return false;
}

bool MacFileSystem::WriteFile(const std::string& /*path*/, const std::string& /*content*/) {
    return false;
}

bool MacFileSystem::DeleteFile(const std::string& /*path*/) {
    return false;
}

bool MacFileSystem::CreateDirectory(const std::string& /*path*/) {
    return false;
}

bool MacFileSystem::DeleteDirectory(const std::string& /*path*/) {
    return false;
}

bool MacFileSystem::EnumerateDirectory(const std::string& /*path*/, std::vector<DirEntry>& out_entries) {
    out_entries.clear();
    return false;
}

bool MacFileSystem::Exists(const std::string& /*path*/) {
    return false;
}

bool MacFileSystem::IsDirectory(const std::string& /*path*/) {
    return false;
}

int64_t MacFileSystem::FileSize(const std::string& /*path*/) {
    return -1;
}

std::string MacFileSystem::GetExecutableDirectory() {
    return std::string();
}

} // namespace macos_
} // namespace platform
} // namespace ava
