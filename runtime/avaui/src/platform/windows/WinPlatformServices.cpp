#include "WinPlatformServices.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

bool WinPlatformServices::OpenFileDialog(std::string& outPath) {
    char buffer[MAX_PATH] = {};

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn)) return false;

    outPath = buffer;
    return true;
}

bool WinPlatformServices::SaveFileDialog(std::string& outPath) {
    char buffer[MAX_PATH] = {};

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameA(&ofn)) return false;

    outPath = buffer;
    return true;
}

void WinPlatformServices::ShowNotification(const std::string& title, const std::string& body) {
    MessageBoxA(nullptr, body.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

void WinPlatformServices::OpenUri(const std::string& uri) {
    ShellExecuteA(nullptr, "open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}
}
}
}
