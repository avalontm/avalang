#ifndef AVA_UI_PLATFORM_WINDOWS_WINWINDOW_H
#define AVA_UI_PLATFORM_WINDOWS_WINWINDOW_H

#include "../../../../avalang/platform/interfaces/services/ui/IWindow.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Real Win32 top-level window: registers a
// window class on first use, creates a WS_OVERLAPPEDWINDOW HWND on
// Create(), and maps WindowState <-> ShowWindow()/SetWindowPlacement().
//
// PumpMessages() is an extra, non-virtual method (not part of IWindow)
// that drains the calling thread's message queue -- callers that hold
// a WinWindow directly (not through the IWindow interface) can drive
// their own render loop with it; every Win32 window still needs *some*
// thread pumping its queue or it will hang/gray-out, this is that hook.
class WinWindow final : public ava::platform::ui::IWindow {
public:
    WinWindow();
    ~WinWindow() override;

    WinWindow(const WinWindow&) = delete;
    WinWindow& operator=(const WinWindow&) = delete;

    void Create(int width, int height, const char* title) override;
    void Destroy() override;

    void Resize(int width, int height) override;
    void Show() override;
    void Hide() override;

    ava::platform::ui::WindowState State() const override;
    void SetState(ava::platform::ui::WindowState state) override;

    void* NativeHandle() const override { return hwnd_; }

    // Returns false once WM_QUIT / WM_CLOSE has been observed for this
    // window -- caller should stop rendering and Destroy() at that point.
    bool PumpMessages();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static const wchar_t* ClassName();
    static void EnsureClassRegistered(HINSTANCE instance);

    HWND hwnd_ = nullptr;
    bool closed_ = false;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINWINDOW_H
