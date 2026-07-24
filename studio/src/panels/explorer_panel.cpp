#include "panels/explorer_panel.h"

#include <filesystem>

#include "imgui.h"

namespace fs = std::filesystem;

namespace studio {

namespace {

void DrawDirectory(const fs::path& dir, std::optional<std::string>& clicked) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        ImGui::TextDisabled("(carpeta no encontrada: %s)", dir.string().c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const auto& path = entry.path();
        if (entry.is_directory()) {
            if (ImGui::TreeNode(path.filename().string().c_str())) {
                DrawDirectory(path, clicked);
                ImGui::TreePop();
            }
        } else if (path.extension() == ".ava") {
            if (ImGui::Selectable(path.filename().string().c_str())) {
                clicked = path.string();
            }
        }
    }
}

} // namespace

std::optional<std::string> DrawExplorerPanel(ExplorerState& state) {
    std::optional<std::string> clicked;

    ImGui::Begin("Explorer");
    ImGui::TextDisabled("%s", state.root_dir.c_str());
    ImGui::Separator();
    DrawDirectory(state.root_dir, clicked);
    ImGui::End();

    return clicked;
}

} // namespace studio
