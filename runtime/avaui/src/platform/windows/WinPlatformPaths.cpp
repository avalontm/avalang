#include "platform/windows/WinPlatformPaths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

std::string WinPlatformPaths::FontsDir() const {
    char sysPath[MAX_PATH];
    GetWindowsDirectoryA(sysPath, sizeof(sysPath));
    return std::string(sysPath) + "\\Fonts";
}

std::string WinPlatformPaths::SystemDir() const {
    char sysPath[MAX_PATH];
    GetWindowsDirectoryA(sysPath, sizeof(sysPath));
    return std::string(sysPath) + "\\System32";
}

std::string WinPlatformPaths::AppDataDir() const {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        return std::string(appdata);
    }
    return std::string();
}

}
}
}
}
