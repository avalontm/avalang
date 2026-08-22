#ifndef AVA_PLATFORM_BAREKERNEL_FILESYSTEM_H
#define AVA_PLATFORM_BAREKERNEL_FILESYSTEM_H

#include "../interfaces/IFileSystem.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelFileSystem : public IFileSystem {
public:
    bool ReadFile(const avastd::string& path, avastd::string& out_content) override;
    bool WriteFile(const avastd::string& path, const avastd::string& content) override;
    bool DeleteFile(const avastd::string& path) override;

    bool CreateDirectory(const avastd::string& path) override;
    bool DeleteDirectory(const avastd::string& path) override;
    bool EnumerateDirectory(const avastd::string& path, avastd::vector<DirEntry>& out_entries) override;

    bool Exists(const avastd::string& path) override;
    bool IsDirectory(const avastd::string& path) override;
    int64_t FileSize(const avastd::string& path) override;

    avastd::string GetExecutableDirectory() override;
};

}
}
}

#endif
