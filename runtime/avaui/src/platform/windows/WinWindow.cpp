#include "WinWindow.h"
#include <string>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

const wchar_t* WinWindow::ClassName() {
    return L"AvaUIWindowClass";
}

void WinWindow::EnsureClassRegistered(HINSTANCE instance) {
    static bool registered = false;
    if (registered) return;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &WinWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = ClassName();

    RegisterClassExW(&wc);
    registered = true;
}

LRESULT CALLBACK WinWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WinWindow* self = reinterpret_cast<WinWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            break;
        }
        case WM_CLOSE:
        case WM_DESTROY:
            if (self) self->closed_ = true;
            if (msg == WM_DESTROY) PostQuitMessage(0);
            break;
        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

WinWindow::WinWindow() = default;

WinWindow::~WinWindow() {
    Destroy();
}

void WinWindow::Create(int width, int height, const char* title) {
    if (hwnd_) return;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    EnsureClassRegistered(instance);

    std::wstring wtitle;
    if (title) {
        int len = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        wtitle.resize(len > 0 ? static_cast<size_t>(len - 1) : 0);
        if (len > 0) {
            MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), len);
        }
    }

    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(
        0, ClassName(), wtitle.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, this
    );

    closed_ = (hwnd_ == nullptr);
}

void WinWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void WinWindow::Resize(int width, int height) {
    if (!hwnd_) return;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE);
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

void WinWindow::Show() {
    if (hwnd_) ShowWindow(hwnd_, SW_SHOW);
}

void WinWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

ava::platform::ui::WindowState WinWindow::State() const {
    if (!hwnd_) return ava::platform::ui::WindowState::Normal;

    if (IsIconic(hwnd_)) return ava::platform::ui::WindowState::Minimized;
    if (IsZoomed(hwnd_)) return ava::platform::ui::WindowState::Maximized;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    if ((style & WS_OVERLAPPEDWINDOW) == 0) {
        // Fullscreen is implemented as a borderless window covering the
        // monitor -- see SetState(); a plain WS_OVERLAPPEDWINDOW loss is
        // the signal we used to enter it.
        return ava::platform::ui::WindowState::Fullscreen;
    }
    return ava::platform::ui::WindowState::Normal;
}

void WinWindow::SetState(ava::platform::ui::WindowState state) {
    if (!hwnd_) return;

    switch (state) {
        case ava::platform::ui::WindowState::Normal:
            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            ShowWindow(hwnd_, SW_RESTORE);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            break;

        case ava::platform::ui::WindowState::Minimized:
            ShowWindow(hwnd_, SW_MINIMIZE);
            break;

        case ava::platform::ui::WindowState::Maximized:
            ShowWindow(hwnd_, SW_MAXIMIZE);
            break;

        case ava::platform::ui::WindowState::Fullscreen: {
            HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi{};
            mi.cbSize = sizeof(MONITORINFO);
            GetMonitorInfoW(monitor, &mi);

            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(hwnd_, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED);
            break;
        }
    }
}

bool WinWindow::PumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            closed_ = true;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !closed_;
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
