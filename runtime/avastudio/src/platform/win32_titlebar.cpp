#include "platform/win32_titlebar.h"

#if defined(_WIN32)

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <windowsx.h>
#include <commdlg.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

namespace studio::titlebar {

namespace {

WNDPROC g_prev_wndproc = nullptr;
int g_titlebar_height = 36;
RECT g_btn_min{};
RECT g_btn_max{};
RECT g_btn_close{};

constexpr int kBorderThickness = 6;
constexpr int kMaxExtraRects = 8;
RECT g_extra_rects[kMaxExtraRects]{};
int g_extra_rect_count = 0;

LRESULT HitTestCaption(HWND hwnd, POINT screen_pt) {
    RECT window_rect;
    GetWindowRect(hwnd, &window_rect);

    const bool on_left = screen_pt.x < window_rect.left + kBorderThickness;
    const bool on_right = screen_pt.x >= window_rect.right - kBorderThickness;
    const bool on_top = screen_pt.y < window_rect.top + kBorderThickness;
    const bool on_bottom = screen_pt.y >= window_rect.bottom - kBorderThickness;

    if (on_top && on_left) return HTTOPLEFT;
    if (on_top && on_right) return HTTOPRIGHT;
    if (on_bottom && on_left) return HTBOTTOMLEFT;
    if (on_bottom && on_right) return HTBOTTOMRIGHT;
    if (on_left) return HTLEFT;
    if (on_right) return HTRIGHT;
    if (on_top) return HTTOP;
    if (on_bottom) return HTBOTTOM;

    POINT client_pt = screen_pt;
    ScreenToClient(hwnd, &client_pt);
    if (PtInRect(&g_btn_min, client_pt) || PtInRect(&g_btn_max, client_pt) || PtInRect(&g_btn_close, client_pt)) {
        return HTCLIENT;
    }
    for (int i = 0; i < g_extra_rect_count; ++i) {
        if (PtInRect(&g_extra_rects[i], client_pt)) {
            return HTCLIENT;
        }
    }

    if (client_pt.y >= 0 && client_pt.y < g_titlebar_height) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wparam == TRUE) {
                return 0;
            }
            break;

        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            return HitTestCaption(hwnd, pt);
        }

        case WM_NCACTIVATE:
            return TRUE;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (monitor) {
                MONITORINFO mi{sizeof(MONITORINFO)};
                if (GetMonitorInfo(monitor, &mi)) {
                    mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                    mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                    mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                    mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                }
            }
            break;
        }

        default:
            break;
    }

    return CallWindowProc(g_prev_wndproc, hwnd, msg, wparam, lparam);
}

}

void Install(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) {
        return;
    }

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_CAPTION;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    g_prev_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

}

void UpdateHitRegions(int titlebar_height, Rect minimize_btn, Rect maximize_btn, Rect close_btn,
                      const Rect* extra_rects, int extra_count) {
    g_titlebar_height = titlebar_height;
    g_btn_min = RECT{minimize_btn.left, minimize_btn.top, minimize_btn.right, minimize_btn.bottom};
    g_btn_max = RECT{maximize_btn.left, maximize_btn.top, maximize_btn.right, maximize_btn.bottom};
    g_btn_close = RECT{close_btn.left, close_btn.top, close_btn.right, close_btn.bottom};

    g_extra_rect_count = 0;
    if (extra_rects) {
        for (int i = 0; i < extra_count && g_extra_rect_count < kMaxExtraRects; ++i) {
            g_extra_rects[g_extra_rect_count++] =
                RECT{extra_rects[i].left, extra_rects[i].top, extra_rects[i].right, extra_rects[i].bottom};
        }
    }
}

bool IsWindowMaximizedNow(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) {
        return false;
    }
    WINDOWPLACEMENT wp{sizeof(WINDOWPLACEMENT)};
    GetWindowPlacement(hwnd, &wp);
    return wp.showCmd == SW_SHOWMAXIMIZED;
}

void OpenUrl(const char* url) {

    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

bool OpenFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir) {
    char buf[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window ? glfwGetWin32Window(window) : nullptr;
    ofn.lpstrFilter = "AvaLang Scripts (*.ava)\0*.ava\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initial_dir.empty() ? nullptr : initial_dir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) {
        return false;
    }
    out_path = buf;
    return true;
}

bool SaveFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir) {
    char buf[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window ? glfwGetWin32Window(window) : nullptr;
    ofn.lpstrFilter = "AvaLang Scripts (*.ava)\0*.ava\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "ava";
    ofn.lpstrInitialDir = initial_dir.empty() ? nullptr : initial_dir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&ofn)) {
        return false;
    }
    out_path = buf;
    return true;
}

bool OpenFolderDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir) {
    HWND owner = window ? glfwGetWin32Window(window) : nullptr;

    const bool co_initialized =
        SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));

    bool ok = false;
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(L"Open Folder");

        if (!initial_dir.empty()) {
            wchar_t wdir[MAX_PATH]{};
            MultiByteToWideChar(CP_ACP, 0, initial_dir.c_str(), -1, wdir, MAX_PATH);
            IShellItem* start_folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(wdir, nullptr, IID_PPV_ARGS(&start_folder)))) {
                dialog->SetFolder(start_folder);
                start_folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* result = nullptr;
            if (SUCCEEDED(dialog->GetResult(&result))) {
                PWSTR path_w = nullptr;
                if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path_w))) {
                    char buf[MAX_PATH] = "";
                    WideCharToMultiByte(CP_ACP, 0, path_w, -1, buf, MAX_PATH, nullptr, nullptr);
                    out_path = buf;
                    ok = true;
                    CoTaskMemFree(path_w);
                }
                result->Release();
            }
        }
        dialog->Release();
    }

    if (co_initialized) {
        CoUninitialize();
    }
    return ok;
}

namespace {

bool IsDirectoryPath(const std::string& path) {
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

}

void RevealInFileExplorer(const std::string& path) {
    if (path.empty()) return;
    if (IsDirectoryPath(path)) {

        ShellExecuteA(nullptr, "explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {

        const std::string param = "/select,\"" + path + "\"";
        ShellExecuteA(nullptr, "open", "explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

}

#else

namespace studio::titlebar {

void Install(GLFWwindow*) {}
void UpdateHitRegions(int, Rect, Rect, Rect, const Rect*, int) {}
bool IsWindowMaximizedNow(GLFWwindow*) { return false; }
void OpenUrl(const char*) {}
bool OpenFileDialog(GLFWwindow*, std::string&, const std::string&) { return false; }
bool SaveFileDialog(GLFWwindow*, std::string&, const std::string&) { return false; }
bool OpenFolderDialog(GLFWwindow*, std::string&, const std::string&) { return false; }
void RevealInFileExplorer(const std::string&) {}

}

#endif
