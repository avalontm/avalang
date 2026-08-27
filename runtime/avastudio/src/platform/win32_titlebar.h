#pragma once

#include <string>

struct GLFWwindow;

namespace studio::titlebar {

void Install(GLFWwindow* window);

struct Rect {
    int left = 0, top = 0, right = 0, bottom = 0;
};

void UpdateHitRegions(int titlebar_height, Rect minimize_btn, Rect maximize_btn, Rect close_btn,
                      const Rect* extra_rects = nullptr, int extra_count = 0);

bool IsWindowMaximizedNow(GLFWwindow* window);

void OpenUrl(const char* url);

// `filter` is a Win32-style double-null-terminated filter string ("Label\0*.ext\0...\0\0");
// pass nullptr for the default AvaLang Scripts (*.ava) filter this always had before. Callers
// browsing for something that isn't a .ava file (ava_cli.exe, an AES key blob, ...) should pass
// their own filter -- see BuildBrowseField::kAvaCliPath/kKeyFile in main.cpp for examples.
bool OpenFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "",
                    const char* filter = nullptr);
bool SaveFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

bool OpenFolderDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

void RevealInFileExplorer(const std::string& path);

}
