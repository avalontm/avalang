#ifndef AVA_PLATFORM_MAC_FILESYSTEM_H
#define AVA_PLATFORM_MAC_FILESYSTEM_H

#include "../interfaces/IFileSystem.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB (Phase 6). Not yet backed by real
// POSIX/Darwin calls -- see Windows implementation in platform/windows/
// for the target behavior each method must eventually match.
class MacFileSystem : public IFileSystem {
public:
    bool ReadFile(const std::string& path, std::string& out_content) override;
    bool WriteFile(const std::string& path, const std::string& content) override;
    bool DeleteFile(const std::string& path) override;

    bool CreateDirectory(const std::string& path) override;
    bool DeleteDirectory(const std::string& path) override;
    bool EnumerateDirectory(const std::string& path, std::vector<DirEntry>& out_entries) override;

    bool Exists(const std::string& path) override;
    bool IsDirectory(const std::string& path) override;
    int64_t FileSize(const std::string& path) override;

    std::string GetExecutableDirectory() override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_FILESYSTEM_H
