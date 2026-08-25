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

bool OpenFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");
bool SaveFileDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

bool OpenFolderDialog(GLFWwindow* window, std::string& out_path, const std::string& initial_dir = "");

void RevealInFileExplorer(const std::string& path);

}
