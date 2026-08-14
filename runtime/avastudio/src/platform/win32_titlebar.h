#pragma once

#include <string>

// Custom Win32 title bar: removes the OS-native caption/border (the grey
// Windows title bar) while keeping native move/resize/Aero-Snap/minimize/
// maximize behavior, so Ava Studio can draw its own VSCode-style dark
// title bar with ImGui instead. Windows-only -- on other platforms these
// calls are no-ops and GLFW's normal decorated window (with its native
// title bar) is used unchanged.
//
// How it works (the same trick used by Windows Terminal and other modern
// Win32 apps with a custom frame):
//   1. WM_NCCALCSIZE returns an empty non-client area, so the whole
//      window becomes "client area" -- no OS title bar or border gets
//      painted.
//   2. WM_NCHITTEST is answered manually so Windows still treats the
//      strip we draw our own title bar in as HTCAPTION (drag to move,
//      double-click to maximize, right-click for the system menu,
//      drag-to-edge for Aero Snap) and the outermost few pixels as
//      resize handles (HTLEFT/HTRIGHT/HTTOP/...), while the area over
//      our own minimize/maximize/close buttons stays HTCLIENT so ImGui
//      receives the click normally instead of it being swallowed as a
//      caption click.
//   3. DwmExtendFrameIntoClientArea keeps the native drop shadow (and,
//      on Windows 11, rounded corners) even though there's no
//      non-client frame left to carry it.

struct GLFWwindow;

namespace studio::titlebar {

// Call once, right after the GLFWwindow is created.
void Install(GLFWwindow* window);

// A clickable region in screen coordinates -- the same space as
// ImGui::GetItemRectMin/Max() on the main viewport, which is what the
// title bar panel hands back.
struct Rect {
    int left = 0, top = 0, right = 0, bottom = 0;
};

// Call every frame after drawing the custom title bar, so WM_NCHITTEST
// knows (a) how tall the draggable caption strip is, and (b) where all
// the real, clickable widgets living inside that strip are (minimize/
// maximize/close plus anything else, like the File/Run menu buttons),
// so clicks on them reach ImGui instead of being swallowed as a caption
// drag. `extra_rects`/`extra_count` covers any additional buttons beyond
// the three window-chrome ones -- every clickable widget drawn inside
// the titlebar height must be listed here or Windows will treat clicks
// on it as "drag the window" instead of forwarding them.
void UpdateHitRegions(int titlebar_height, Rect minimize_btn, Rect maximize_btn, Rect close_btn,
                      const Rect* extra_rects = nullptr, int extra_count = 0);

// True while the window is maximized -- lets the title bar draw the
// "restore" icon (two overlapping squares) instead of the maximize icon
// (one square), matching VSCode/Windows conventions.
bool IsWindowMaximizedNow(GLFWwindow* window);

// Opens `url` in the OS default browser (e.g. from the About dialog's
// GitHub link). No-op on platforms where this isn't implemented.
void OpenUrl(const char* url);

// Native "Open"/"Save As" file dialogs, filtered to .ava scripts (with an
// "all files" fallback). Return false if the user cancelled.
bool OpenFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");
bool SaveFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

// Native "Open Folder" picker (VSCode-style: pick a directory to use as
// the project's working directory / Explorer root), rather than a single
// file. Uses the modern Windows Common Item Dialog restricted to folders,
// the same look as OpenFileDialog above. Returns false if the user
// cancelled.
bool OpenFolderDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

// Reveals `path` in the OS file manager (Windows Explorer): opens its
// containing folder with the item itself pre-selected/highlighted, same
// as VSCode's "Reveal in File Explorer" / "Open Containing Folder". If
// `path` is itself a folder, opens that folder's own contents instead of
// selecting it inside its parent. No-op on platforms where this isn't
// implemented (see file header comment).
void RevealInFileExplorer(const std::string& path);

} // namespace studio::titlebar
