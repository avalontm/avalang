#pragma once

#include "imgui.h"

namespace studio {

// Loads JetBrains Mono (bundled into the exe, see fonts/jetbrains_mono_regular_ttf.h)
// and installs it as ImGui's default font.
//
// Call this once, after ImGui::CreateContext() and before the first
// ImGui::NewFrame() -- in practice, right after `ImGuiIO& io = ImGui::GetIO();`
// in main(). Returns the loaded font (rarely needed by the caller; ImGui
// widgets pick it up automatically via io.FontDefault).
ImFont* LoadDefaultFont(float size_px = 16.0f);

// Loads JetBrains Mono Bold (bundled into the exe, see
// fonts/jetbrains_mono_bold_ttf.h) as an ADDITIONAL font -- unlike
// LoadDefaultFont(), this does NOT touch io.FontDefault, so the rest of
// the UI (menus, buttons, panels) stays on the regular weight. Only the
// Code Editor panel opts into it, via GetCodeFont().
//
// Call once, same timing constraints as LoadDefaultFont(): after
// ImGui::CreateContext() and before the first ImGui_ImplOpenGL3_Init()
// atlas build.
ImFont* LoadBoldFont(float size_px = 16.0f);

// Returns the font loaded by LoadBoldFont(), or nullptr if it hasn't been
// called yet. editor_panel.cpp wraps TextEditor::Render() in
// PushFont(GetCodeFont())/PopFont() with this.
ImFont* GetCodeFont();

} // namespace studio
