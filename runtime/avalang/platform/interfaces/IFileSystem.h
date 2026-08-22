#ifndef AVA_PLATFORM_IFILESYSTEM_H
#define AVA_PLATFORM_IFILESYSTEM_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IFileSystem.
#include "PAL_ABI.h"

#include "../barekernel/stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {

struct DirEntry {
    avastd::string name;
    bool is_directory = false;
};

// Abstracts file/directory access. No OS API may be called outside the
// concrete implementations of this interface (windows/, linux/, macos/).
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual bool ReadFile(const avastd::string& path, avastd::string& out_content) = 0;
    virtual bool WriteFile(const avastd::string& path, const avastd::string& content) = 0;
    virtual bool DeleteFile(const avastd::string& path) = 0;

    virtual bool CreateDirectory(const avastd::string& path) = 0;
    virtual bool DeleteDirectory(const avastd::string& path) = 0;
    virtual bool EnumerateDirectory(const avastd::string& path, avastd::vector<DirEntry>& out_entries) = 0;

    virtual bool Exists(const avastd::string& path) = 0;
    virtual bool IsDirectory(const avastd::string& path) = 0;
    virtual int64_t FileSize(const avastd::string& path) = 0;

    // Returns the directory containing the current executable.
    // Used by Extern/FFI system to locate modules/ directory.
    virtual avastd::string GetExecutableDirectory() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IFILESYSTEM_H