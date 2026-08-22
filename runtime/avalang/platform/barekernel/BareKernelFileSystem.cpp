#include "BareKernelFileSystem.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"

namespace ava {
namespace platform {
namespace barekernel {

static avastd::string kExecDir = "/";

bool BareKernelFileSystem::ReadFile(const avastd::string& path, avastd::string& out) {
    int fd = ckm_open(path.c_str(), CKM_O_RDONLY, 0);
    if (fd < 0) return false;
    out.clear();
    char buf[4096];
    long n = 0;
    while ((n = ckm_read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, static_cast<size_t>(n));
    }
    ckm_close(fd);
    return n == 0;
}

bool BareKernelFileSystem::WriteFile(const avastd::string& path, const avastd::string& content) {
    int fd = ckm_open(path.c_str(), CKM_O_WRONLY | CKM_O_CREAT | CKM_O_TRUNC, CKM_S_IRWXU);
    if (fd < 0) return false;
    long total = 0;
    while (total < (long)content.size()) {
        long n = ckm_write(fd, content.data() + total, (long)content.size() - total);
        if (n <= 0) { ckm_close(fd); return false; }
        total += n;
    }
    return ckm_close(fd) == 0;
}

bool BareKernelFileSystem::DeleteFile(const avastd::string& path) {
    return ckm_unlink(path.c_str()) == 0;
}

bool BareKernelFileSystem::CreateDirectory(const avastd::string& path) {
    return ckm_mkdir(path.c_str()) == 0;
}

bool BareKernelFileSystem::DeleteDirectory(const avastd::string& path) {
    return ckm_rmdir(path.c_str()) == 0;
}

bool BareKernelFileSystem::EnumerateDirectory(const avastd::string& path, avastd::vector<DirEntry>& out) {
#if CKM_CAP_DIR_ENUM
    void* h = ckm_opendir(path.c_str());
    if (!h) return false;
    CkmDirEntry e;
    while (ckm_readdir(h, &e) == 0) {
        DirEntry entry;
        entry.name = e.name;
        entry.is_directory = e.is_directory != 0;
        out.push_back(avastd::move(entry));
    }
    ckm_closedir(h);
    return true;
#else
    (void)path; (void)out;
    return false;
#endif
}

bool BareKernelFileSystem::Exists(const avastd::string& path) {
    CkmStat st;
    return ckm_stat(path.c_str(), &st) == 0;
}

bool BareKernelFileSystem::IsDirectory(const avastd::string& path) {
    CkmStat st;
    if (ckm_stat(path.c_str(), &st) != 0) return false;
    return st.is_directory != 0;
}

int64_t BareKernelFileSystem::FileSize(const avastd::string& path) {
    CkmStat st;
    if (ckm_stat(path.c_str(), &st) != 0) return -1;
    return static_cast<int64_t>(st.size);
}

avastd::string BareKernelFileSystem::GetExecutableDirectory() {
    return kExecDir;
}

}
}
}
