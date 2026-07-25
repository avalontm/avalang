#include "panels/explorer_panel.h"

#include <filesystem>
#include <fstream>

#include "imgui.h"
#include "palette.h"

namespace fs = std::filesystem;

namespace studio {

namespace {

struct CreateRequest {
    bool open = false;
    bool is_folder = false;
    std::string target_dir;
};

CreateRequest g_create_request;
char g_name_buf[128] = "";

struct DeleteRequest {
    bool open = false;
    std::string path;
};

DeleteRequest g_delete_request;

ImU32 IconColorFor(const fs::path& path, bool is_dir) {
    if (is_dir) return palette::U32FromHex(0xDCB67A); // folder amber, VSCode-ish
    const std::string ext = path.extension().string();
    if (ext == ".ava") return palette::U32FromHex(palette::kPrimary);
    if (ext == ".md") return palette::U32FromHex(0x6A9FD8);
    if (ext == ".json") return palette::U32FromHex(0xD8B96A);
    return palette::U32FromHex(palette::kTextMuted);
}

// Small colored square drawn inline before the label -- a lightweight
// stand-in for per-extension file-type icons (VSCode-style) without
// needing an icon font/texture atlas.
void DrawIcon(ImU32 color) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight();
    const float s = h * 0.55f;
    const float y = p.y + (h - s) * 0.5f;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + s, y + s), color, 2.0f);
    ImGui::Dummy(ImVec2(s, h));
    ImGui::SameLine(0.0f, 6.0f);
}

void DrawCreatePopup() {
    if (g_create_request.open) {
        ImGui::OpenPopup("##CreateEntry");
        g_name_buf[0] = '\0';
        g_create_request.open = false;
    }
    if (ImGui::BeginPopup("##CreateEntry")) {
        ImGui::TextDisabled("%s", g_create_request.is_folder ? "New Folder" : "New File");
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter =
            ImGui::InputText("##name", g_name_buf, sizeof(g_name_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        const bool create_clicked = ImGui::Button("Create");
        if ((enter || create_clicked) && g_name_buf[0] != '\0') {
            std::error_code ec;
            fs::path target = fs::path(g_create_request.target_dir) / g_name_buf;
            if (g_create_request.is_folder) {
                fs::create_directories(target, ec);
            } else {
                if (target.extension().empty()) target += ".ava";
                fs::create_directories(target.parent_path(), ec);
                std::ofstream(target.string(), std::ios::binary).close();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// "Delete" needs an explicit Yes/Cancel confirmation -- unlike creating a
// file/folder, deleting one is destructive and can't be undone from
// Explorer, so a stray click shouldn't be able to remove a script outright.
// Actually removes the file from disk on confirm and reports it via
// `result.file_deleted` so the caller (main.cpp) can close its tab if the
// file happened to be open in the editor.
void DrawDeleteConfirmPopup(ExplorerResult& result) {
    if (g_delete_request.open) {
        ImGui::OpenPopup("Delete File?");
        g_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f));
    if (ImGui::BeginPopupModal("Delete File?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path target(g_delete_request.path);
        ImGui::TextWrapped("Delete \"%s\"?", target.filename().string().c_str());
        ImGui::TextDisabled("This can't be undone.");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
            std::error_code ec;
            fs::remove(target, ec);
            result.file_deleted = g_delete_request.path;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// Right-click context menu for a *file* row: same New File/New Folder
// (relative to its parent dir, so you can add a sibling script) as the
// folder context menu, plus Delete for this specific file.
void DrawFileContextMenu(const std::string& file_path, const std::string& dir) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New File")) g_create_request = {true, false, dir};
        if (ImGui::MenuItem("New Folder")) g_create_request = {true, true, dir};
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            g_delete_request = {true, file_path};
        }
        ImGui::EndPopup();
    }
}

// Right-click context menu ("Nuevo archivo" / "Nueva carpeta") attached to
// whatever item was drawn immediately before this call.
void DrawContextMenu(const std::string& dir) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New File")) g_create_request = {true, false, dir};
        if (ImGui::MenuItem("New Folder")) g_create_request = {true, true, dir};
        ImGui::EndPopup();
    }
}

void DrawDirectory(const fs::path& dir, ExplorerResult& result) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        ImGui::TextDisabled("(folder not found: %s)", dir.string().c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const auto& path = entry.path();
        ImGui::PushID(path.string().c_str());
        if (entry.is_directory()) {
            DrawIcon(IconColorFor(path, true));
            const bool open = ImGui::TreeNode(path.filename().string().c_str());
            DrawContextMenu(path.string());
            if (open) {
                DrawDirectory(path, result);
                ImGui::TreePop();
            }
        } else {
            DrawIcon(IconColorFor(path, false));
            // ImGuiSelectableFlags_AllowDoubleClick makes Selectable()
            // return true on both single and double clicks -- the
            // IsMouseDoubleClicked() check below is what actually gates
            // opening the file, so a single click only highlights the
            // row (VSCode-style) instead of immediately swapping the
            // active editor tab out from under you.
            if (ImGui::Selectable(path.filename().string().c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    result.file_to_open = path.string();
                }
            }
            DrawFileContextMenu(path.string(), dir.string());
        }
        ImGui::PopID();
    }
}

} // namespace

ExplorerResult DrawExplorerPanel(ExplorerState& state) {
    ExplorerResult result;

    ImGui::Begin("Explorer");
    ImGui::TextDisabled("%s", state.root_dir.c_str());
    ImGui::Separator();
    DrawDirectory(state.root_dir, result);

    // Right-click on empty space (not on a file/folder row) creates at the
    // listing's root instead of inside whatever the last node happened
    // to be.
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("##RootContext");
    }
    if (ImGui::BeginPopup("##RootContext")) {
        if (ImGui::MenuItem("New File")) g_create_request = {true, false, state.root_dir};
        if (ImGui::MenuItem("New Folder")) g_create_request = {true, true, state.root_dir};
        ImGui::EndPopup();
    }
    DrawCreatePopup();
    DrawDeleteConfirmPopup(result);
    ImGui::End();

    return result;
}

} // namespace studio
