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

struct RenameRequest {
    bool open = false;
    std::string path;
};

RenameRequest g_rename_request;
char g_rename_buf[128] = "";

// True if `child` is `parent` itself or lives somewhere underneath it.
// Compared path-component-by-path-component so "/" vs "\\" or a trailing
// separator don't matter. Used to (a) stop a folder being dragged inside
// its own subtree, and (b) clear the selection when the selected entry
// was inside a folder that just got deleted.
bool PathContains(const fs::path& parent, const fs::path& child) {
    auto pit = parent.begin();
    auto cit = child.begin();
    for (; pit != parent.end(); ++pit, ++cit) {
        if (cit == child.end() || *pit != *cit) return false;
    }
    return true;
}

// Moves `src_path` (file or folder) on disk into `dest_dir`, keeping its
// current name. Used by every drag-and-drop drop target below. Reports
// the move via `result.file_renamed` -- same field a manual Rename uses --
// so the caller's existing tab-retargeting logic (main.cpp ->
// studio::RenameTabPath) handles it identically either way.
void PerformMove(const std::string& src_path, const std::string& dest_dir, ExplorerState& state,
                  ExplorerResult& result) {
    if (src_path.empty() || dest_dir.empty()) return;
    const fs::path src(src_path);
    const fs::path dest_parent(dest_dir);
    const fs::path new_path = dest_parent / src.filename();

    if (new_path == src) return; // dropped back onto its own parent -- no-op

    std::error_code ec;
    if (fs::is_directory(src, ec) && PathContains(src, dest_parent)) {
        return; // can't move a folder inside itself or one of its own children
    }
    if (fs::exists(new_path, ec)) return; // don't clobber an existing entry with the same name

    fs::rename(src, new_path, ec);
    if (!ec) {
        result.file_renamed = std::make_pair(src_path, new_path.string());
        if (state.selected_path == src_path) state.selected_path = new_path.string();
    }
}

ImU32 IconColorFor(const fs::path& path, bool is_dir) {
    if (is_dir) return palette::U32FromHex(0xDCB67A); // folder amber, VSCode-ish
    const std::string ext = path.extension().string();
    if (ext == ".ava") return palette::U32FromHex(palette::kPrimary);
    if (ext == ".avaui") return palette::U32FromHex(palette::kAccentGold);
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
void DrawDeleteConfirmPopup(ExplorerState& state, ExplorerResult& result) {
    if (g_delete_request.open) {
        ImGui::OpenPopup("Delete?");
        g_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path target(g_delete_request.path);
        std::error_code type_ec;
        const bool is_dir = fs::is_directory(target, type_ec);

        if (is_dir) {
            ImGui::TextWrapped("Delete folder \"%s\" and everything inside it?",
                                target.filename().string().c_str());
        } else {
            ImGui::TextWrapped("Delete \"%s\"?", target.filename().string().c_str());
        }
        ImGui::TextDisabled("This can't be undone.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // Split the popup's own width evenly instead of two fixed-size
        // buttons floating at the left -- keeps them centered and
        // balanced regardless of how long the filename made the window.
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float button_w = (ImGui::GetContentRegionAvail().x - spacing) / 2.0f;

        // Destructive action gets a subtle red so it doesn't read as a
        // plain "OK" -- Cancel stays the neutral default look.
        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kError, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(0xc93b3b));
        if (ImGui::Button("Delete", ImVec2(button_w, 0.0f))) {
            std::error_code ec;
            if (is_dir) {
                fs::remove_all(target, ec);
            } else {
                fs::remove(target, ec);
            }
            result.file_deleted = g_delete_request.path;
            // Clear the selection if it was the deleted entry itself, or
            // (for a deleted folder) anything that used to live inside it.
            if (state.selected_path == g_delete_request.path ||
                (is_dir && PathContains(target, fs::path(state.selected_path)))) {
                state.selected_path.clear();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button("Cancel", ImVec2(button_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// "Rename" pre-fills the popup with the file's current name (not its full
// path) so the user is just editing the base name, same as VSCode/Explorer/
// Finder. Renames on disk immediately on confirm (no separate "are you
// sure?" step, unlike Delete -- a rename is easy to undo by renaming back)
// and reports {old, new} via `result.file_renamed` so the caller (main.cpp)
// can retarget the file's tab if it's open in the editor.
void DrawRenamePopup(ExplorerState& state, ExplorerResult& result) {
    if (g_rename_request.open) {
        ImGui::OpenPopup("##RenameEntry");
        fs::path current(g_rename_request.path);
        std::snprintf(g_rename_buf, sizeof(g_rename_buf), "%s", current.filename().string().c_str());
        g_rename_request.open = false;
    }
    if (ImGui::BeginPopup("##RenameEntry")) {
        ImGui::TextDisabled("Rename");
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        const bool enter =
            ImGui::InputText("##rename_name", g_rename_buf, sizeof(g_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        const bool rename_clicked = ImGui::Button("Rename");
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

// Right-click context menu shared by file *and* folder rows: New
// File/New Folder relative to `dir` (a file's parent, so you add a
// sibling script; a folder's own path, so you add something inside it),
// plus Rename/Delete for `entry_path` itself -- either kind of entry
// supports both today. The "F2"/"Del" hints are just labels here -- the
// actual hotkeys are handled once in DrawExplorerPanel (see the comment
// there), gated on the panel having focus, so they work no matter which
// row's context menu (if any) is open.
void DrawEntryContextMenu(const std::string& entry_path, const std::string& dir) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New File")) g_create_request = {true, false, dir};
        if (ImGui::MenuItem("New Folder")) g_create_request = {true, true, dir};
        ImGui::Separator();
        if (ImGui::MenuItem("Rename", "F2")) {
            g_rename_request = {true, entry_path};
        }
        if (ImGui::MenuItem("Delete", "Del")) {
            g_delete_request = {true, entry_path};
        }
        ImGui::EndPopup();
    }
}

void DrawDirectory(const fs::path& dir, ExplorerState& state, ExplorerResult& result) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        ImGui::TextDisabled("(folder not found: %s)", dir.string().c_str());
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
            // A plain click both selects the row *and* (via TreeNodeEx's
            // own default behavior) toggles it open/closed, same as
            // before -- this just also records the selection so F2/Del
            // and the highlight above act on folders too, not just files.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                state.selected_path = path_str;
            }
            // Drag source: pick this folder up to move it elsewhere.
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("EXPLORER_PATH", path_str.c_str(), path_str.size() + 1);
                ImGui::TextUnformatted(path.filename().string().c_str());
                ImGui::EndDragDropSource();
            }
            // Drop target: dropping a file/folder here moves it inside
            // this folder, even while it's collapsed -- same as VSCode.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_PATH")) {
                    const std::string src_path(static_cast<const char*>(payload->Data));
                    PerformMove(src_path, path_str, state, result);
                }
                ImGui::EndDragDropTarget();
            }
            DrawEntryContextMenu(path_str, path_str);
            if (open) {
                DrawDirectory(path, state, result);
                ImGui::TreePop();
            }
        } else {
            DrawIcon(IconColorFor(path, false));
            const bool is_selected = (state.selected_path == path_str);
            // ImGuiSelectableFlags_AllowDoubleClick makes Selectable()
            // return true on both single and double clicks -- the
            // IsMouseDoubleClicked() check below is what actually gates
            // opening the file, so a single click only selects/highlights
            // the row (VSCode-style) instead of immediately swapping the
            // active editor tab out from under you. That selection is
            // also what F2/Del act on (see the hotkey handling in
            // DrawExplorerPanel), so a click always updates it even when
            // it lands on the double-click that opens the file too.
            if (ImGui::Selectable(path.filename().string().c_str(), is_selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selected_path = path_str;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    result.file_to_open = path_str;
                }
            }
            // Right-click also selects the row, same as left-click --
            // otherwise "Rename"/"Delete" from the context menu could act
            // on whatever was selected before, not the row you just
            // right-clicked.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                state.selected_path = path_str;
            }
            // Drag source: pick this file up to move it elsewhere.
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("EXPLORER_PATH", path_str.c_str(), path_str.size() + 1);
                ImGui::TextUnformatted(path.filename().string().c_str());
                ImGui::EndDragDropSource();
            }
            // Drop target: dropping something onto a file moves it next
            // to that file, i.e. into the file's own containing folder.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_PATH")) {
                    const std::string src_path(static_cast<const char*>(payload->Data));
                    PerformMove(src_path, dir.string(), state, result);
                }
                ImGui::EndDragDropTarget();
            }
            DrawEntryContextMenu(path_str, dir.string());
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
    DrawDirectory(state.root_dir, state, result);

    // Empty space below the tree (not any file/folder row) fills the rest
    // of the panel: right-click there creates at the listing's root
    // instead of inside whatever the last node happened to be, and
    // dragging a file/folder there moves it back up to the project root
    // -- both need a real item to hang off of, hence the Dummy.
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
        if (ImGui::MenuItem("New File")) g_create_request = {true, false, state.root_dir};
        if (ImGui::MenuItem("New Folder")) g_create_request = {true, true, state.root_dir};
        ImGui::EndPopup();
    }

    // F2 (rename) / Del (delete) act on whatever row is currently
    // selected -- file or folder, same as Explorer/VSCode -- checked once
    // here rather than per-row so they fire from anywhere in the panel,
    // not just while hovering the selected row itself. Gated on the
    // Explorer window having focus so these don't fire while e.g. the
    // Code Editor panel has focus and the user is just typing/deleting
    // text there; that focus also naturally goes away on its own while a
    // popup (rename, delete confirm, ...) is open, so this can't
    // re-trigger itself. exists() (rather than just "selected_path is
    // non-empty") guards against a stale selection surviving the entry
    // being deleted or renamed out from under it by some other means.
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

    DrawCreatePopup();
    DrawDeleteConfirmPopup(state, result);
    DrawRenamePopup(state, result);
    ImGui::End();

    return result;
}

} // namespace studio
