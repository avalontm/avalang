#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "TextEditor.h"
#include "design/design_document.h"
#include "languages/class_index.h"
#include "languages/fold_index.h"
#include "languages/function_index.h"
#include "languages/member_access_resolver.h"
#include "languages/workspace_index.h"
#include "panels/properties_panel.h"
#include "util/log_bridge.h"

namespace studio {

enum class TabViewMode { Code, Design };

struct EditorTab {

    int id = 0;

    std::string file_path;
    std::string modules_path;
    TextEditor editor;
    TextEditor::Trie autocomplete_trie;
    TextEditor::AutoCompleteConfig autocomplete_config;
    FunctionIndex function_index;

    ClassIndex class_index;

    VariableTypeIndex variable_type_index;

    FoldIndex fold_index;
    std::unordered_set<int> folded_lines;

    // Per-tab text zoom applied to the code editor only (mouse wheel while
    // holding Ctrl, or Ctrl+=/Ctrl+-/Ctrl+0), 1.0 = 100%. See ClampEditorZoom
    // and the zoom handling right before tab.editor.Render() in
    // DrawEditorPanel (editor_panel.cpp).
    float zoom = 1.0f;

    bool dirty = false;

    bool index_dirty = false;
    double last_edit_time = 0.0;

    bool is_welcome = false;

    bool is_avaui = false;
    TabViewMode view_mode = TabViewMode::Code;
    design::DesignDocument design;

    std::string avaui_load_error;

    // Keyboard-selected row for the bespoke "just typed a dot" member popup
    // (DrawDotCompletionPopup in editor_panel.cpp). Reset to 0 whenever the caret
    // position it was computed for changes, so a stale index never survives into a
    // different (or filtered) member list.
    int dot_popup_selected = 0;
    int dot_popup_line = -1;
    int dot_popup_column = -1;

    // Text position of the last right-click inside the editor, used by the context menu (see
    // DrawEditorPanel) so "Ir a la definición" etc. act on what was actually clicked instead of
    // the blinking text cursor, which the vendored editor does not move on right-click.
    bool context_menu_click_valid = false;
    TextEditor::CursorPosition context_menu_click_pos;

    // One entry per interface, in a class heritage clause declared in *this*
    // buffer (ClassInfo::source_file.empty()), that the class doesn't fully
    // implement, recomputed alongside class_index in RebuildIndexAndTrie.
    // Drives the yellow "missing interface members" squiggle under that
    // interface's name (VS/Roslyn style: `class C : IShape` underlines
    // IShape, not C) and the right-click "Implementar interfaz" quick-fix --
    // see DrawIncompleteInterfaceSquiggles and ImplementMissingInterfaceMembers
    // in editor_panel.cpp.
    struct IncompleteInterface {
        std::string class_name;
        std::string interface_name;
        int line = 0;
        std::vector<ClassMember> missing_members;
    };
    std::vector<IncompleteInterface> incomplete_interfaces;

    // Interface names this tab contributed to languages::KnownInterfaceNames
    // (the tokenizer's shared, reference-counted table) as of the last
    // RebuildIndexAndTrie. Diffed against the freshly computed set on the
    // next rebuild -- and handed to languages::UpdateKnownInterfaceNames as
    // "release everything" on tab close -- so one tab renaming, deleting, or
    // closing an interface never steals its color out from under another
    // open tab that still declares it. See RebuildIndexAndTrie and
    // CloseTabNow in editor_panel.cpp.
    std::unordered_set<std::string> known_interface_names;

    // Generation of languages::KnownInterfaceNamesGeneration() this tab's
    // editor was last colorized against. -1 so the very first render always
    // forces a colorize (see the staleness check next to tab.editor.Render()
    // in DrawEditorPanel). Kept per-tab, not global, because each tab's
    // editor caches its own colorization -- one tab catching up doesn't mean
    // another one has.
    int colored_interface_generation = -1;

    // Same mechanism as known_interface_names/colored_interface_generation
    // above, for variable names contributed to
    // languages::KnownVariableRefCounts (see RebuildIndexAndTrie and
    // CloseTabNow in editor_panel.cpp).
    std::unordered_set<std::string> known_variable_names;
    int colored_variable_generation = -1;

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

    bool save_as_requested = false;
    bool open_settings_panel_requested = false;

    bool build_requested = false;

    bool quick_open_requested = false;

    bool new_project_requested = false;

    std::optional<PropertiesState> designer_selection;

    std::string project_root;

    std::string modules_path;

    WorkspaceIndex workspace_index;

    LogBridge* log_bridge = nullptr;

    EditorTab* Active();
    const EditorTab* Active() const;

    int focus_tab_id = -1;

    bool code_editor_has_focus = false;

    bool goto_definition_requested = false;
    std::string goto_definition_file;
    int goto_definition_line = 0;
    int goto_definition_column = 0;
};

void InitEditorPanel(EditorState& state);

EditorTab& OpenFileInTab(EditorState& state, const std::string& path);

EditorTab& NewUntitledTab(EditorState& state);

EditorTab& OpenWelcomeTab(EditorState& state);

void SaveActiveTab(EditorState& state);

void SaveTab(EditorState& state, EditorTab& tab);

bool HasUnsavedChanges(const EditorState& state);

void SaveAllTabs(EditorState& state);

void HighlightError(EditorState& state, const std::string& file_path, int line, int column,
                     const std::string& message);

void ClearErrorHighlights(EditorState& state);

void SelectMatchInEditor(EditorState& state, const std::string& file_path, int line, int column_start,
                          int column_end);

void RequestCloseTab(EditorState& state, int index);

void CloseTabForPath(EditorState& state, const std::string& path);

void RenameTabPath(EditorState& state, const std::string& old_path, const std::string& new_path);

void ToggleTabViewMode(EditorState& state, EditorTab& tab);

void ToggleFold(EditorTab& tab, int start_line);

bool IsLineFolded(const EditorTab& tab, int start_line);

void FoldAll(EditorTab& tab);

void UnfoldAll(EditorTab& tab);

void DrawEditorPanel(EditorState& state);

}
