#include "panels/explorer_panel.h"

#include <filesystem>
#include <fstream>

#include "imgui.h"
#include "palette.h"
#include "util/i18n.h"
#include "util/scaffold_templates.h"

namespace fs = std::filesystem;

namespace studio {

namespace {

std::string TrFormat(const std::string& key, const std::string& arg) {
    const std::string& fmt = util::Tr(key);
    const size_t pos = fmt.find("%s");
    if (pos == std::string::npos) return fmt;
    return fmt.substr(0, pos) + arg + fmt.substr(pos + 2);
}

struct CreateRequest {
    bool open = false;
    bool is_folder = false;
    std::string target_dir;
};

CreateRequest g_create_request;
char g_name_buf[128] = "";

// Fase 5 ("Generar"): which boilerplate to scaffold when creating a file
// (irrelevant for folders). Reset to kClass every time the popup opens
// (DrawCreatePopup below), same "explicit default, not whatever was left
// over from last time" reasoning CommandPaletteState/QuickOpenState already
// follow for their own transient UI state.
util::ScaffoldKind g_new_file_kind = util::ScaffoldKind::kClass;

struct DeleteRequest {
    bool open = false;
    std::string path;
};

DeleteRequest g_delete_request;

struct RenameRequest {
    bool open = false;
    std::string path;
};

RenameRequest g_rename_request;
char g_rename_buf[128] = "";

bool PathContains(const fs::path& parent, const fs::path& child) {
    auto pit = parent.begin();
    auto cit = child.begin();
    for (; pit != parent.end(); ++pit, ++cit) {
        if (cit == child.end() || *pit != *cit) return false;
    }
    return true;
}

void PerformMove(const std::string& src_path, const std::string& dest_dir, ExplorerState& state,
                  ExplorerResult& result) {
    if (src_path.empty() || dest_dir.empty()) return;
    const fs::path src(src_path);
    const fs::path dest_parent(dest_dir);
    const fs::path new_path = dest_parent / src.filename();

    if (new_path == src) return;

    std::error_code ec;
    if (fs::is_directory(src, ec) && PathContains(src, dest_parent)) {
        return;
    }
    if (fs::exists(new_path, ec)) return;

    fs::rename(src, new_path, ec);
    if (!ec) {
        result.file_renamed = std::make_pair(src_path, new_path.string());
        if (state.selected_path == src_path) state.selected_path = new_path.string();
    }
}

ImU32 IconColorFor(const fs::path& path, bool is_dir) {
    if (is_dir) return palette::U32FromHex(0xDCB67A);
    const std::string ext = path.extension().string();
    if (ext == ".ava") return palette::U32FromHex(palette::kPrimary);
    if (ext == ".avaui") return palette::U32FromHex(palette::kAccentGold);
    if (ext == ".md") return palette::U32FromHex(0x6A9FD8);
    if (ext == ".json") return palette::U32FromHex(0xD8B96A);
    return palette::U32FromHex(palette::kTextMuted);
}

void DrawIcon(ImU32 color) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight();
    const float s = h * 0.55f;
    const float y = p.y + (h - s) * 0.5f;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + s, y + s), color, 2.0f);
    ImGui::Dummy(ImVec2(s, h));
    ImGui::SameLine(0.0f, 6.0f);
}

void DrawCreatePopup(ExplorerResult& result) {
    if (g_create_request.open) {
        ImGui::OpenPopup("##CreateEntry");
        g_name_buf[0] = '\0';
        g_new_file_kind = util::ScaffoldKind::kClass;
        g_create_request.open = false;
    }
    if (ImGui::BeginPopup("##CreateEntry")) {
        ImGui::TextDisabled(
            "%s", (g_create_request.is_folder ? util::Tr("explorer.new_folder") : util::Tr("explorer.new_file"))
                      .c_str());

        // Only meaningful for files -- a folder has no boilerplate to pick.
        // Two radios rather than a combo: same reasoning as the Obfuscate/
        // Zero-disk checkboxes in Build, this is a two-way, always-visible
        // choice, not a longer list that would benefit from collapsing.
        if (!g_create_request.is_folder) {
            bool is_class = g_new_file_kind == util::ScaffoldKind::kClass;
            if (ImGui::RadioButton(util::Tr("explorer.new_file_kind_class").c_str(), is_class)) {
                g_new_file_kind = util::ScaffoldKind::kClass;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(util::Tr("explorer.new_file_kind_screen").c_str(), !is_class)) {
                g_new_file_kind = util::ScaffoldKind::kScreen;
            }
        }

        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter =
            ImGui::InputText("##name", g_name_buf, sizeof(g_name_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        const bool create_clicked = ImGui::Button(util::Tr("explorer.create").c_str());
        if ((enter || create_clicked) && g_name_buf[0] != '\0') {
            std::error_code ec;
            fs::path target = fs::path(g_create_request.target_dir) / g_name_buf;
            if (g_create_request.is_folder) {
                fs::create_directories(target, ec);
            } else {
                // Extension is always forced to match `g_new_file_kind`,
                // even if the user typed a different one (or none) --
                // whatever the user types is just the stem; the boilerplate
                // written below always matches the extension on disk, so
                // the two can never disagree (a ".avaui" with .ava-style
                // class code inside would be a silent foot-gun otherwise).
                target.replace_extension(util::ScaffoldExtension(g_new_file_kind));
                fs::create_directories(target.parent_path(), ec);
                std::ofstream out(target.string(), std::ios::binary);
                out << util::BuildScaffoldContent(g_new_file_kind, target.stem().string());
                out.close();
                // Auto-open the new file, same as double-clicking it in the
                // tree would -- reuses the existing file_to_open field
                // instead of inventing a new result/mechanism, same "no
                // separate source of truth" reasoning the rest of this
                // plan's phases already follow.
                result.file_to_open = target.string();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawDeleteConfirmPopup(ExplorerState& state, ExplorerResult& result) {

    const std::string delete_title = util::Tr("explorer.delete_title") + "##DeleteConfirm";
    if (g_delete_request.open) {
        ImGui::OpenPopup(delete_title.c_str());
        g_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal(delete_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path target(g_delete_request.path);
        std::error_code type_ec;
        const bool is_dir = fs::is_directory(target, type_ec);

        if (is_dir) {
            ImGui::TextWrapped(
                "%s", TrFormat("explorer.delete_folder_confirm", target.filename().string()).c_str());
        } else {
            ImGui::TextWrapped("%s", TrFormat("explorer.delete_file_confirm", target.filename().string()).c_str());
        }
        ImGui::TextDisabled("%s", util::Tr("explorer.delete_undone").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float button_w = (ImGui::GetContentRegionAvail().x - spacing) / 2.0f;

        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kError, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(0xc93b3b));
        if (ImGui::Button(util::Tr("explorer.delete").c_str(), ImVec2(button_w, 0.0f))) {
            std::error_code ec;
            if (is_dir) {
                fs::remove_all(target, ec);
            } else {
                fs::remove(target, ec);
            }
            result.file_deleted = g_delete_request.path;

            if (state.selected_path == g_delete_request.path ||
                (is_dir && PathContains(target, fs::path(state.selected_path)))) {
                state.selected_path.clear();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button(util::Tr("common.cancel").c_str(), ImVec2(button_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawRenamePopup(ExplorerState& state, ExplorerResult& result) {
    if (g_rename_request.open) {
        ImGui::OpenPopup("##RenameEntry");
        fs::path current(g_rename_request.path);
        std::snprintf(g_rename_buf, sizeof(g_rename_buf), "%s", current.filename().string().c_str());
        g_rename_request.open = false;
    }
    if (ImGui::BeginPopup("##RenameEntry")) {
        ImGui::TextDisabled("%s", util::Tr("explorer.rename").c_str());
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        const bool enter =
            ImGui::InputText("##rename_name", g_rename_buf, sizeof(g_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        const bool rename_clicked = ImGui::Button(util::Tr("explorer.rename").c_str());
        if ((enter || rename_clicked) && g_rename_buf[0] != '\0') {
            fs::path old_path(g_rename_request.path);
            fs::path new_path = old_path.parent_path() / g_rename_buf;
            if (new_path != old_path) {
                std::error_code ec;
                fs::rename(old_path, new_path, ec);
                if (!ec) {
                    result.file_renamed = std::make_pair(old_path.string(), new_path.string());
                    state.selected_path = new_path.string();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DrawEntryContextMenu(const std::string& entry_path, const std::string& dir, ExplorerResult& result) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(util::Tr("explorer.new_file").c_str())) g_create_request = {true, false, dir};
        if (ImGui::MenuItem(util::Tr("explorer.new_folder").c_str())) g_create_request = {true, true, dir};
        ImGui::Separator();
        if (ImGui::MenuItem(util::Tr("explorer.open_in_file_manager").c_str())) {
            result.reveal_in_file_manager = entry_path;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(util::Tr("explorer.rename").c_str(), "F2")) {
            g_rename_request = {true, entry_path};
        }
        if (ImGui::MenuItem(util::Tr("explorer.delete").c_str(), "Del")) {
            g_delete_request = {true, entry_path};
        }
        ImGui::EndPopup();
    }
}

void DrawDirectory(const fs::path& dir, ExplorerState& state, ExplorerResult& result) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        ImGui::TextDisabled("%s", TrFormat("explorer.folder_not_found", dir.string()).c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const auto& path = entry.path();
        const std::string path_str = path.string();
        ImGui::PushID(path_str.c_str());
        if (entry.is_directory()) {
            DrawIcon(IconColorFor(path, true));
            const bool is_selected = (state.selected_path == path_str);
            const bool open = ImGui::TreeNodeEx(path.filename().string().c_str(),
                                                 is_selected ? ImGuiTreeNodeFlags_Selected : 0);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                state.selected_path = path_str;
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("EXPLORER_PATH", path_str.c_str(), path_str.size() + 1);
                ImGui::TextUnformatted(path.filename().string().c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_PATH")) {
                    const std::string src_path(static_cast<const char*>(payload->Data));
                    PerformMove(src_path, path_str, state, result);
                }
                ImGui::EndDragDropTarget();
            }
            DrawEntryContextMenu(path_str, path_str, result);
            if (open) {
                DrawDirectory(path, state, result);
                ImGui::TreePop();
            }
        } else {
            DrawIcon(IconColorFor(path, false));
            const bool is_selected = (state.selected_path == path_str);

            if (ImGui::Selectable(path.filename().string().c_str(), is_selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selected_path = path_str;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    result.file_to_open = path_str;
                }
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                state.selected_path = path_str;
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("EXPLORER_PATH", path_str.c_str(), path_str.size() + 1);
                ImGui::TextUnformatted(path.filename().string().c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_PATH")) {
                    const std::string src_path(static_cast<const char*>(payload->Data));
                    PerformMove(src_path, dir.string(), state, result);
                }
                ImGui::EndDragDropTarget();
            }
            DrawEntryContextMenu(path_str, dir.string(), result);
        }
        ImGui::PopID();
    }
}

}

ExplorerResult DrawExplorerPanel(ExplorerState& state, bool* p_open) {
    ExplorerResult result;

    const std::string title = util::Tr("panel.explorer.title") + "###explorer";
    ImGui::Begin(title.c_str(), p_open);
    ImGui::TextDisabled("%s", state.root_dir.c_str());
    ImGui::Separator();
    DrawDirectory(state.root_dir, state, result);

    ImVec2 trailing_space = ImGui::GetContentRegionAvail();
    if (trailing_space.y < 20.0f) trailing_space.y = 20.0f;
    ImGui::Dummy(trailing_space);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("##RootContext");
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_PATH")) {
            const std::string src_path(static_cast<const char*>(payload->Data));
            PerformMove(src_path, state.root_dir, state, result);
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopup("##RootContext")) {
        if (ImGui::MenuItem(util::Tr("explorer.new_file").c_str())) g_create_request = {true, false, state.root_dir};
        if (ImGui::MenuItem(util::Tr("explorer.new_folder").c_str())) g_create_request = {true, true, state.root_dir};
        ImGui::EndPopup();
    }

    std::error_code selection_ec;
    if (ImGui::IsWindowFocused() && !state.selected_path.empty() &&
        fs::exists(state.selected_path, selection_ec)) {
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            g_rename_request = {true, state.selected_path};
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            g_delete_request = {true, state.selected_path};
        }
    }

    DrawCreatePopup(result);
    DrawDeleteConfirmPopup(state, result);
    DrawRenamePopup(state, result);
    ImGui::End();

    return result;
}

}
