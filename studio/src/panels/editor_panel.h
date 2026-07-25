#pragma once

#include <memory>
#include <string>
#include <vector>

#include "TextEditor.h"
#include "languages/function_index.h"

namespace studio {

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
    bool dirty = false;

    // True only for the startup "Welcome" tab (see OpenWelcomeTab) -- it
    // has no file and isn't a code buffer, so DrawEditorPanel renders a
    // static welcome screen for it instead of the TextEditor widget.
    bool is_welcome = false;

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

// Draws the Code Editor panel (center dock): a VSCode-like tab strip --
// reorderable, closable, unsaved-changes dot -- above the active tab's
// editor (line numbers, AvaLang syntax highlighting, keyword/built-in
// autocomplete). Also renders the close-confirmation modal when a dirty
// tab's close was requested.
void DrawEditorPanel(EditorState& state);

} // namespace studio