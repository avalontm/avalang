#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "TextEditor.h"
#include "design/design_document.h"
#include "languages/class_index.h"
#include "languages/diagnostics_engine.h"
#include "languages/fold_index.h"
#include "languages/function_index.h"
#include "languages/member_access_resolver.h"
#include "languages/workspace_index.h"
#include "panels/problems_panel.h"
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

    float zoom = 1.0f;

    bool minimap_dragging = false;

    bool dirty = false;

    bool index_dirty = false;
    double last_edit_time = 0.0;

    bool format_pending = false;

    bool is_welcome = false;

    bool is_avaui = false;
    TabViewMode view_mode = TabViewMode::Code;
    design::DesignDocument design;

    std::string avaui_load_error;

    int dot_popup_selected = 0;
    int dot_popup_line = -1;
    int dot_popup_column = -1;

    bool context_menu_click_valid = false;
    TextEditor::CursorPosition context_menu_click_pos;

    struct IncompleteInterface {
        std::string class_name;
        std::string interface_name;
        int line = 0;
        std::vector<ClassMember> missing_members;
    };
    std::vector<IncompleteInterface> incomplete_interfaces;

    std::unordered_set<std::string> known_interface_names;

    int colored_interface_generation = -1;

    std::unordered_set<std::string> known_variable_names;
    int colored_variable_generation = -1;

    std::unordered_set<std::string> known_class_names;
    int colored_class_generation = -1;

    std::vector<diagnostics::Diagnostic> diagnostics;
    std::vector<diagnostics::InlayHint> inlay_hints;

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

    bool show_minimap = true;

    bool show_inlay_hints = true;

    bool format_on_save = false;

    bool format_on_type = false;

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

void FormatTab(EditorState& state, EditorTab& tab);

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

std::vector<ProblemEntry> CollectDiagnosticProblems(const EditorState& state);

}