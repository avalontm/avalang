#include "WinClipboard.h"
#include <windows.h>
#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

void WinClipboard::SetText(const std::string& text) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;

    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, static_cast<wchar_t*>(dst), wlen);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        } else {
            GlobalFree(mem);
        }
    }

    CloseClipboard();
}

std::string WinClipboard::GetText() const {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
    if (!OpenClipboard(nullptr)) return {};

    std::string result;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const wchar_t* wtext = static_cast<const wchar_t*>(GlobalLock(handle));
        if (wtext) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wtext, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                result.resize(static_cast<size_t>(len - 1));
                WideCharToMultiByte(CP_UTF8, 0, wtext, -1, result.data(), len, nullptr, nullptr);
            }
            GlobalUnlock(handle);
        }
    }

    CloseClipboard();
    return result;
}

bool WinClipboard::HasText() const {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
