#pragma once

#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinWindow {
public:
    WinWindow();
    ~WinWindow();

    WinWindow(const WinWindow&) = delete;
    WinWindow& operator=(const WinWindow&) = delete;

    void Create(int width, int height, const char* title);
    void Destroy();

    void Resize(int width, int height);
    void Show();
    void Hide();

    void* NativeHandle() const { return hwnd_; }

    bool PumpMessages();
    bool IsClosed() const { return closed_; }

    unsigned int CurrentDpi() const { return dpi_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static const wchar_t* ClassName();
    static void EnsureClassRegistered(HINSTANCE instance);
    static void EnsureProcessDpiAware();

    HWND hwnd_ = nullptr;
    bool closed_ = false;
    unsigned int dpi_ = 96;
};

}
}
}
}