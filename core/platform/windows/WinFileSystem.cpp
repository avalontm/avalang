#include "WinFileSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows.h #defines DeleteFile/CreateDirectory as ANSI/Unicode dispatch
// macros (-> DeleteFileA/CreateDirectoryA here), which would otherwise
// rewrite the out-of-line definitions below (e.g. WinFileSystem::DeleteFile)
// to names the header never declared. Undefine them; the explicit *A calls
// inside this file already spell out the ANSI entry points directly.
#undef DeleteFile
#undef CreateDirectory

namespace ava {
namespace platform {
namespace windows {

bool WinFileSystem::ReadFile(const std::string& path, std::string& out_content) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        return false;
    }

    out_content.resize(static_cast<size_t>(size.QuadPart));

    DWORD bytes_read = 0;
    BOOL ok = TRUE;
    if (size.QuadPart > 0) {
        ok = ::ReadFile(h, &out_content[0], static_cast<DWORD>(size.QuadPart), &bytes_read, nullptr);
    }

    CloseHandle(h);

    if (!ok || static_cast<LONGLONG>(bytes_read) != size.QuadPart) {
        return false;
    }
    return true;
}

bool WinFileSystem::WriteFile(const std::string& path, const std::string& content) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytes_written = 0;
    BOOL ok = TRUE;
    if (!content.empty()) {
        ok = ::WriteFile(h, content.data(), static_cast<DWORD>(content.size()), &bytes_written, nullptr);
    }

    CloseHandle(h);

    if (!ok || bytes_written != content.size()) {
        return false;
    }
    return true;
}

bool WinFileSystem::DeleteFile(const std::string& path) {
    return ::DeleteFileA(path.c_str()) != 0;
}

bool WinFileSystem::CreateDirectory(const std::string& path) {
    if (::CreateDirectoryA(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WinFileSystem::DeleteDirectory(const std::string& path) {
    return ::RemoveDirectoryA(path.c_str()) != 0;
}

bool WinFileSystem::EnumerateDirectory(const std::string& path, std::vector<DirEntry>& out_entries) {
    out_entries.clear();

    std::string search_pattern = path;
    if (!search_pattern.empty() && search_pattern.back() != '\\' && search_pattern.back() != '/') {
        search_pattern += '\\';
    }
    search_pattern += "*";

    WIN32_FIND_DATAA find_data{};
    HANDLE find_handle = FindFirstFileA(search_pattern.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        std::string name = find_data.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        DirEntry entry;
        entry.name = name;
        entry.is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        out_entries.push_back(entry);
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);
    return true;
}

bool WinFileSystem::Exists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool WinFileSystem::IsDirectory(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int64_t WinFileSystem::FileSize(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        return -1;
    }
    LARGE_INTEGER size;
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return static_cast<int64_t>(size.QuadPart);
}

std::string WinFileSystem::GetExecutableDirectory() {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    std::string exe_path(buf, len);

    // Remove executable name, keep only directory
    size_t last_slash = exe_path.find_last_of('\\');
    if (last_slash != std::string::npos) {
        return exe_path.substr(0, last_slash);
    }
    return exe_path;
}

} // namespace windows
} // namespace platform
} // namespace ava