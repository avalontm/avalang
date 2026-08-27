#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TextEditor.h"
#include "design/design_document.h"
#include "languages/class_index.h"
#include "languages/function_index.h"
#include "languages/member_access_resolver.h"
#include "panels/properties_panel.h"
#include "util/log_bridge.h"

namespace studio {

enum class TabViewMode { Code, Design };

struct EditorTab {

    int id = 0;

    std::string file_path;
    TextEditor editor;
    TextEditor::Trie autocomplete_trie;
    TextEditor::AutoCompleteConfig autocomplete_config;
    FunctionIndex function_index;

    ClassIndex class_index;

    VariableTypeIndex variable_type_index;

    bool dirty = false;

    bool is_welcome = false;

    bool is_avaui = false;
    TabViewMode view_mode = TabViewMode::Code;
    design::DesignDocument design;

    std::string avaui_load_error;

    std::string GetText() const { return editor.GetText(); }
    void SetText(const std::string& text) { editor.SetText(text); }

    std::string DisplayName() const;
};

struct EditorState {
    std::vector<std::unique_ptr<EditorTab>> tabs;
    int active_tab = -1;
    int next_tab_id = 1;

    int pending_close_index = -1;

    bool run_requested = false;
    bool run_project_requested = false;
    bool check_requested = false;
    bool find_in_project_requested = false;
    bool save_requested = false;
    bool close_tab_requested = false;
    bool new_tab_requested = false;
    bool open_requested = false;
    bool open_folder_requested = false;

    // Fase 3 (Command Palette): three actions that today are only reachable
    // from the title bar (TitleBarResult::save_as_requested/
    // open_settings_requested/build_requested) get their own EditorState
    // flags here, same pattern as run_requested/check_requested/etc. above,
    // so the Command Palette can trigger them without going through the
    // title bar at all. main.cpp ORs these into the same handling blocks
    // that already look at the title bar's equivalents, and resets them at
    // the end of the frame alongside the rest of this group.
    bool save_as_requested = false;
    bool open_settings_panel_requested = false;

    // Used to just open the Build panel; now (paths-only panel, build kicks off from the Run
    // menu) this fires an actual build -- same shape as run_project_requested/check_requested
    // above, handled alongside them in main.cpp instead of near open_settings_panel_requested.
    bool build_requested = false;

    // Fase 4 (Quick Open): same shape as find_in_project_requested above --
    // lets the Command Palette's "Edit: Quick Open" entry trigger it without
    // going through the title bar's Edit menu at all.
    bool quick_open_requested = false;

    // Fase 6 (New Project wizard): same shape as quick_open_requested above
    // -- lets the Command Palette's "File: New Project" entry trigger it
    // without going through the title bar's File menu at all.
    bool new_project_requested = false;

    std::optional<PropertiesState> designer_selection;

    std::string project_root;

    LogBridge* log_bridge = nullptr;

    EditorTab* Active();
    const EditorTab* Active() const;

    int focus_tab_id = -1;

    // Set every frame by DrawEditorPanel right after the active tab's
    // TextEditor::Render() call (via ImGui::IsItemFocused() on the child
    // window it just submitted). Used to gate the global "Find in Project"
    // shortcut (Ctrl+Shift+F): the ImGuiColorTextEdit widget already binds
    // Ctrl+Shift+F to "select all occurrences in this file" internally
    // (handleKeyboardInputs(), only when its own child window has focus), so
    // without this flag both would fire on the same keypress -- same kind of
    // collision as want_run/Shift+F5 in Fase 1 and want_build/Ctrl+Shift+B in
    // Fase 2, just gated on focus instead of a modifier key.
    bool code_editor_has_focus = false;
};

void InitEditorPanel(EditorState& state);

EditorTab& OpenFileInTab(EditorState& state, const std::string& path);

EditorTab& NewUntitledTab(EditorState& state);

EditorTab& OpenWelcomeTab(EditorState& state);

void SaveActiveTab(EditorState& state);

void SaveTab(EditorTab& tab);

bool HasUnsavedChanges(const EditorState& state);

void SaveAllTabs(EditorState& state);

void HighlightError(EditorState& state, const std::string& file_path, int line, int column,
                     const std::string& message);

void ClearErrorHighlights(EditorState& state);

// Jumps to a search-result match: selects [column_start, column_end) on
// `line` (0-based columns, same convention as TextEditor::SelectRegion) and
// scrolls it into view. Deliberately NOT HighlightError -- that paints the
// red "error" marker and gets wiped by the next Run/Check, which would be
// wrong for a Find in Project hit that isn't an error. No-op if the file
// isn't open in a tab yet; callers open it first (same two-step pattern
// already used for Terminal/Problems file-click handling in main.cpp).
void SelectMatchInEditor(EditorState& state, const std::string& file_path, int line, int column_start,
                          int column_end);

void RequestCloseTab(EditorState& state, int index);

void CloseTabForPath(EditorState& state, const std::string& path);

void RenameTabPath(EditorState& state, const std::string& old_path, const std::string& new_path);

void ToggleTabViewMode(EditorTab& tab);

void DrawEditorPanel(EditorState& state);

}
