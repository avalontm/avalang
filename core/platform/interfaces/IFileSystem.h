#ifndef AVA_PLATFORM_IFILESYSTEM_H
#define AVA_PLATFORM_IFILESYSTEM_H

#include <string>
#include <vector>
#include <cstdint>

namespace ava {
namespace platform {

struct DirEntry {
    std::string name;
    bool is_directory = false;
};

// Abstracts file/directory access. No OS API may be called outside the
// concrete implementations of this interface (windows/, linux/, macos/).
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual bool ReadFile(const std::string& path, std::string& out_content) = 0;
    virtual bool WriteFile(const std::string& path, const std::string& content) = 0;
    virtual bool DeleteFile(const std::string& path) = 0;

    virtual bool CreateDirectory(const std::string& path) = 0;
    virtual bool DeleteDirectory(const std::string& path) = 0;
    virtual bool EnumerateDirectory(const std::string& path, std::vector<DirEntry>& out_entries) = 0;

    virtual bool Exists(const std::string& path) = 0;
    virtual bool IsDirectory(const std::string& path) = 0;
    virtual int64_t FileSize(const std::string& path) = 0;

    // Returns the directory containing the current executable.
    // Used by Extern/FFI system to locate modules/ directory.
    virtual std::string GetExecutableDirectory() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IFILESYSTEM_H