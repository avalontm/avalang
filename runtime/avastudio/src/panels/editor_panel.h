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

// Which widget a tab's content area renders: the normal TextEditor
// buffer (Code, the default, every existing .ava/.md/etc. tab), or the
// Design canvas from designer_canvas.h (Design -- only ever set for
// tabs whose EditorTab::is_avaui is true). See DrawEditorPanel's tab
// content dispatch and 08_DESIGNER_VIEW_PLAN.md section 4.
enum class TabViewMode { Code, Design };

// One open buffer/tab in the Code Editor panel. Heap-allocated and owned
// via std::unique_ptr in EditorState::tabs (never stored by value in the
// vector) because autocomplete_config's callback captures `this` tab's
// address, and TextEditor::SetAutoCompleteConfig(&autocomplete_config)
// stores that pointer too -- both must stay valid for the tab's whole
// lifetime, which a reallocating std::vector<EditorTab> would break the
// moment a second tab is opened.
struct EditorTab {
    // Stable identity for this tab's ImGui tab item, independent of its
    // position in EditorState::tabs (which changes as tabs open/close) and
    // independent of its display label (which changes on save-as/rename) --
    // see DrawEditorPanel's use of "##tab%d" as the hidden ImGui ID suffix.
    int id = 0;

    std::string file_path;      // empty = unsaved/untitled buffer
    TextEditor editor;          // owns the buffer, cursors, undo history, colorizer
    TextEditor::Trie autocomplete_trie;
    TextEditor::AutoCompleteConfig autocomplete_config; // must outlive editor.SetAutoCompleteConfig(&this)
    FunctionIndex function_index; // func nombre(params) locales + de imports, para
                                   // autocompletado con nombre real y parameter hints
    ClassIndex class_index;       // clases (locales + importadas, transitivo) para el
                                   // autocompletado por miembros ("instancia." -> say())
    VariableTypeIndex variable_type_index; // variable -> nombre_de_clase, best-effort
                                            // (Fase 2 de TODO_autocompletado_miembros.md)
    bool dirty = false;

    // True only for the startup "Welcome" tab (see OpenWelcomeTab) -- it
    // has no file and isn't a code buffer, so DrawEditorPanel renders a
    // static welcome screen for it instead of the TextEditor widget.
    bool is_welcome = false;

    // True for any tab whose file_path ends in ".avaui" (detected in
    // OpenFileInTab by extension, see 08_DESIGNER_VIEW_PLAN.md section
    // 4) -- these get `design` populated instead of (or in addition to)
    // `editor`, and default to view_mode == Design instead of Code.
    // false for every other tab, which never touch `design` at all.
    bool is_avaui = false;
    TabViewMode view_mode = TabViewMode::Code;
    design::DesignDocument design; // only meaningful when is_avaui is true

    // Set by OpenFileInTab if design::LoadAvauiFile failed to parse an
    // existing .avaui file's contents -- `design` is left as a blank
    // document (design::NewBlankAvauiDocument()) in that case rather
    // than refusing to open the tab, same as opening a corrupt .frm in
    // VS6 still opened *a* form, just an empty one. Empty string = no
    // error (either not a .avaui tab, or it loaded/parsed cleanly).
    // DrawEditorPanel shows this as a dismissable-by-editing banner
    // above the Design canvas.
    std::string avaui_load_error;

    std::string GetText() const { return editor.GetText(); }
    void SetText(const std::string& text) { editor.SetText(text); }

    // Display name for the tab label and the title bar: "Welcome" for the
    // startup tab, the file's base name, or "Untitled" for a buffer that
    // has never been saved.
    std::string DisplayName() const;
};

// Owns every open Code Editor tab (VSCode-style: opening a file that's
// already open focuses its existing tab instead of duplicating it;
// closing a tab leaves the others exactly as they were).
struct EditorState {
    std::vector<std::unique_ptr<EditorTab>> tabs;
    int active_tab = -1;  // index into tabs, or -1 when tabs is empty
    int next_tab_id = 1;

    // Tab a close was requested for but that has unsaved changes --
    // DrawEditorPanel shows a Save/Don't Save/Cancel confirmation for it
    // instead of closing immediately. -1 when no confirmation is pending.
    int pending_close_index = -1;

    // Cross-frame command flags, mirroring the old single-buffer EditorState:
    // set by the editor/titlebar this frame, consumed and cleared by
    // main.cpp's global hotkey handling right after DrawEditorPanel().
    bool run_requested = false;
    bool save_requested = false;
    bool close_tab_requested = false;   // Ctrl+W
    bool new_tab_requested = false;     // Ctrl+N
    bool open_requested = false;        // set by the Welcome tab's "Open File..." action
    bool open_folder_requested = false; // set by the Welcome tab's "Open Folder..." action

    // Set by DrawEditorPanel (cleared to nullopt at the top of every
    // call, then filled in if the active tab is a .avaui in Design view
    // and its canvas registered a click this frame) -- main.cpp reads
    // this right after DrawEditorPanel() the same way it already reads
    // DrawPreviewPanel's return value, so a click on the Design canvas
    // updates the Properties panel exactly like clicking the Preview
    // tree does.
    std::optional<PropertiesState> designer_selection;

    // The fixed root every `.avaui` file's `import "components/x"`
    // resolves against (see design/component_resolver.h's constructor
    // comment) -- set once by main.cpp right after
    // ResolveWorkspaceDir(), same folder the Explorer is rooted at.
    // Empty until main.cpp sets it, which DrawDesignerCanvas treats as
    // "don't resolve components" (see designer_canvas.h) -- so a
    // caller that never sets this just keeps the pre-resolver
    // behavior instead of crashing on an empty base dir.
    std::string project_root;

    // Fase 4 (AVASTUDIO_AVAUI_MIGRATION_PLAN.md): main.cpp's session-
    // wide LogBridge (util/log_bridge.h), set once right alongside
    // `project_root` above. DrawEditorPanel threads it straight through
    // to DrawDesignerCanvas so a failed BuildLiveRender (or a node
    // missing from its uidToRect) shows up in the Output panel, not
    // just the in-canvas banner. Null until main.cpp sets it, which
    // DrawDesignerCanvas treats as "don't log" -- same safe-default
    // pattern as `project_root` being empty.
    LogBridge* log_bridge = nullptr;

    EditorTab* Active();
    const EditorTab* Active() const;

    // Tab `id` (EditorTab::id, not an index -- indices shift as tabs
    // open/close) that DrawEditorPanel should force into focus this
    // frame, e.g. because Explorer was clicked on a file that's already
    // open in a background tab. -1 when nothing is pending. Only setting
    // EditorState::active_tab isn't enough to actually switch the visible
    // tab: ImGui's tab bar tracks its own "selected" tab internally and
    // only reacts to programmatic selection via
    // ImGuiTabItemFlags_SetSelected on the frame it's requested, so
    // DrawEditorPanel consumes this field and clears it right after.
    int focus_tab_id = -1;
};

// One-time setup for the panel as a whole. Call once before the first
// DrawEditorPanel call.
void InitEditorPanel(EditorState& state);

// Opens `path` in a new tab, or focuses its tab if already open. Also
// used for brand-new untitled buffers when `path` is empty.
EditorTab& OpenFileInTab(EditorState& state, const std::string& path);

// Creates a fresh untitled tab (Ctrl+N / File > New File) and focuses it.
EditorTab& NewUntitledTab(EditorState& state);

// Creates Ava Studio's startup "Welcome" tab (VSCode-style landing page:
// quick actions, no code buffer) and focuses it. Only meant to be called
// once, right after InitEditorPanel, before any real file is opened.
EditorTab& OpenWelcomeTab(EditorState& state);

void SaveActiveTab(EditorState& state);

// Writes `tab`'s buffer to disk at its current file_path and clears its
// dirty flag. No-op if file_path is empty (untitled) or the file can't be
// opened for writing -- there's no error surfaced back to the caller in
// either case, same as SaveActiveTab's existing silent-failure behavior.
void SaveTab(EditorTab& tab);

// True if any non-Welcome tab has unsaved changes. Meant for the
// app-exit confirmation in main.cpp (GLFWwindow close, custom titlebar's
// X, and File > Exit all check this before actually closing).
bool HasUnsavedChanges(const EditorState& state);

// Saves every dirty, non-Welcome tab that already has a file_path.
// Untitled dirty tabs (file_path empty) are left untouched -- there's
// nothing to write into without a Save As dialog, which needs the GLFW
// window handle this function doesn't have; the app-exit confirmation in
// main.cpp handles those separately, one Save As dialog at a time.
void SaveAllTabs(EditorState& state);

// Highlights `line`/`column` (1-based; 0 = unknown, e.g. some runtime
// errors -- see core/src/common/ava_error.h) in the tab open on
// `file_path` as a compile/runtime error: tints that line's gutter and
// text red (palette::kError) with `message` as its hover tooltip, and
// moves the caret there so it's visible without hunting for it. No-op if
// `file_path` isn't open in any tab or `line` is 0. Called from
// main.cpp right after a failed EngineBridge::RunScript.
void HighlightError(EditorState& state, const std::string& file_path, int line, int column,
                     const std::string& message);

// Clears any highlight set by HighlightError, on every open tab. Called
// before each run (a previous error's line shouldn't stay red after a
// successful retry) -- SetChangeCallback in InitTab also clears it
// per-tab as soon as that tab's buffer is edited.
void ClearErrorHighlights(EditorState& state);

// Closes `index` immediately if its buffer is clean, or arms the
// Save/Don't Save/Cancel confirmation (resolved inside DrawEditorPanel)
// if it has unsaved changes. Safe to call with any valid index, including
// one that isn't the active tab (e.g. closing a background tab).
void RequestCloseTab(EditorState& state, int index);

// Closes the tab open on `path`, if any, immediately and without the
// usual unsaved-changes confirmation. Meant for when the file was deleted
// out from under the editor (e.g. Explorer's Delete) -- at that point
// there's nothing left on disk to save the buffer back to, so prompting
// "save before closing?" wouldn't make sense. No-op if `path` isn't open.
void CloseTabForPath(EditorState& state, const std::string& path);

// Updates the tab open on `old_path`, if any, to point at `new_path`
// instead -- for when the file was renamed out from under the editor
// (Explorer's Rename). The buffer/undo history/cursor are untouched, only
// EditorTab::file_path changes, so the tab's label (DisplayName()) picks
// up the new name on the next frame and any subsequent Save writes to the
// new location. No-op if `old_path` isn't open.
void RenameTabPath(EditorState& state, const std::string& old_path, const std::string& new_path);

// F7 / "View > Toggle Design View" -- flips `tab.view_mode` between
// Code and Design for a .avaui tab (no-op for any other tab, same as
// VS6 where F7 only meant something with a .frm open). See
// 08_DESIGNER_VIEW_PLAN.md section 4.
//
// Design -> Code: re-serializes `tab.design` (design::WriteAvauiText)
// into `tab.editor`'s buffer, so Code view always reflects the latest
// drag&drop/property edits made in Design view, not stale text from
// whenever the file was last opened or last toggled to Code.
//
// Code -> Design: re-parses `tab.editor`'s current text
// (design::ParseAvauiText) back into `tab.design`, so hand-edits made
// in Code view (e.g. to the `methods`/code-behind section) aren't lost
// when switching back. If the text fails to parse, the toggle is
// aborted (view_mode stays Code, tab.avaui_load_error is set and shown
// as a banner above the TextEditor) rather than silently discarding
// the last valid Design tree or the user's unparseable edit -- fix the
// syntax, then F7 again.
void ToggleTabViewMode(EditorTab& tab);

// Draws the Code Editor panel (center dock): a VSCode-like tab strip --
// reorderable, closable, unsaved-changes dot -- above the active tab's
// editor (line numbers, AvaLang syntax highlighting, keyword/built-in
// autocomplete, member ("instancia.") autocomplete -- both the library's
// native popup via PopulateMemberSuggestions and the hand-drawn
// DrawDotCompletionPopup that covers the instant right after typing the
// '.' itself --, function parameter hints, and a keyword syntax tooltip
// while typing if/while/for/... -- see DrawKeywordHint in
// editor_panel.cpp). Also renders the close-confirmation modal when a
// dirty tab's close was requested.
void DrawEditorPanel(EditorState& state);

} // namespace studio