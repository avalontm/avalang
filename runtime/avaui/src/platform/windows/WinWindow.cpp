#include "WinWindow.h"
#include "WinInputState.h"
#include <shellscalingapi.h>
#include <imm.h>
#include <string>
#include <vector>

#pragma comment(lib, "imm32.lib")

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

const wchar_t* WinWindow::ClassName() {
    return L"AvaUIWindowClass";
}

void WinWindow::EnsureProcessDpiAware() {
    static bool applied = false;
    if (applied) return;
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    applied = true;
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

namespace {

void HandleTouchMessage(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    UINT pointCount = LOWORD(wParam);
    if (pointCount == 0) return;

    std::vector<TOUCHINPUT> inputs(pointCount);
    HTOUCHINPUT handle = reinterpret_cast<HTOUCHINPUT>(lParam);
    if (!GetTouchInputInfo(handle, pointCount, inputs.data(), sizeof(TOUCHINPUT))) {
        CloseTouchInputHandle(handle);
        return;
    }

    std::vector<ava::platform::ui::NativeTouchPoint> points;
    for (const auto& input : inputs) {
        if (input.dwFlags & (TOUCHEVENTF_DOWN | TOUCHEVENTF_MOVE)) {
            POINT pt{input.x / 100, input.y / 100};
            ScreenToClient(hwnd, &pt);
            points.push_back({static_cast<uint64_t>(input.dwID), pt.x, pt.y});
        }
    }

    WinInput_SetTouchPoints(points);
    CloseTouchInputHandle(handle);
}

void HandleImeComposition(HWND hwnd, LPARAM lParam) {
    if (!(lParam & GCS_COMPSTR)) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    LONG byteLen = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
    std::string text;
    if (byteLen > 0) {
        std::vector<wchar_t> buffer(static_cast<size_t>(byteLen) / sizeof(wchar_t));
        ImmGetCompositionStringW(himc, GCS_COMPSTR, buffer.data(), static_cast<DWORD>(byteLen));

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.size()),
                                          nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            text.resize(static_cast<size_t>(utf8Len));
            WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.size()),
                                text.data(), utf8Len, nullptr, nullptr);
        }
    }

    LONG cursor = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
    ImmReleaseContext(hwnd, himc);

    WinInput_SetImeComposition(byteLen > 0, text, static_cast<int>(cursor));
}

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
        case WM_MOUSEWHEEL: {
            float delta = static_cast<float>(static_cast<short>(HIWORD(wParam))) / WHEEL_DELTA;
            WinInput_PushWheelDelta(0.0f, delta);
            break;
        }
        case WM_MOUSEHWHEEL: {
            float delta = static_cast<float>(static_cast<short>(HIWORD(wParam))) / WHEEL_DELTA;
            WinInput_PushWheelDelta(delta, 0.0f);
            break;
        }
        case WM_CHAR:
            WinInput_PushChar(static_cast<unsigned int>(wParam));
            break;
        case WM_TOUCH:
            HandleTouchMessage(hwnd, wParam, lParam);
            break;
        case WM_IME_COMPOSITION:
            HandleImeComposition(hwnd, lParam);
            break;
        case WM_IME_ENDCOMPOSITION:
            WinInput_SetImeComposition(false, std::string(), 0);
            break;
        case WM_DPICHANGED: {
            if (self) self->dpi_ = LOWORD(wParam);
            auto* suggested = reinterpret_cast<RECT*>(lParam);
            if (suggested) {
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            break;
        }
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

    EnsureProcessDpiAware();

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

    if (hwnd_) {
        RegisterTouchWindow(hwnd_, 0);
        dpi_ = GetDpiForWindow(hwnd_);
    }
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

}
}
}
}