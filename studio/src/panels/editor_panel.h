#pragma once

#include <string>

namespace studio {

struct EditorState {
    std::string file_path;   // empty = unsaved/untitled buffer
    std::string text;        // current buffer contents
    bool dirty = false;
    bool run_requested = false; // set true when the user presses the Run button or Ctrl+F5 this frame
    bool save_requested = false;
};

void LoadFileIntoEditor(EditorState& state, const std::string& path);
void SaveEditorToFile(EditorState& state);

// Draws the Code Editor panel (center dock). Milestone 1 uses a plain
// multiline text box -- swap for a real code-editor widget (e.g. a
// Zep/ImGuiColorTextEdit integration) once IntelliSense/highlighting
// become the next milestone.
void DrawEditorPanel(EditorState& state);

} // namespace studio
