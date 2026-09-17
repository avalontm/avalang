#include "shortcuts/shortcut_registry.h"

namespace studio {

ShortcutRegistry& ShortcutRegistry::Instance() {
    static ShortcutRegistry registry;
    return registry;
}

ShortcutRegistry::ShortcutRegistry() {
    Bind(ShortcutId::GotoDefinition, {ImGuiKey_F12, false, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::Save, {ImGuiKey_S, true, false, false, ShortcutScope::Global});
    Bind(ShortcutId::SaveAs, {ImGuiKey_S, true, true, false, ShortcutScope::Global});
    Bind(ShortcutId::NewFile, {ImGuiKey_N, true, false, false, ShortcutScope::Global});
    Bind(ShortcutId::OpenFile, {ImGuiKey_O, true, false, false, ShortcutScope::Global});
    Bind(ShortcutId::CloseTab, {ImGuiKey_W, true, false, false, ShortcutScope::Global});
    Bind(ShortcutId::Run, {ImGuiKey_F5, false, false, false, ShortcutScope::Global});
    Bind(ShortcutId::RunProject, {ImGuiKey_F5, false, true, false, ShortcutScope::Global});
    Bind(ShortcutId::Build, {ImGuiKey_B, true, false, false, ShortcutScope::Global});
    Bind(ShortcutId::Check, {ImGuiKey_B, true, true, false, ShortcutScope::Global});
    Bind(ShortcutId::ToggleView, {ImGuiKey_F7, false, false, false, ShortcutScope::Global});
    Bind(ShortcutId::FindInProject, {ImGuiKey_F, true, true, false, ShortcutScope::EditorUnfocused});
    Bind(ShortcutId::CommandPalette, {ImGuiKey_P, true, true, false, ShortcutScope::Global});
    Bind(ShortcutId::QuickOpen, {ImGuiKey_P, true, false, false, ShortcutScope::Global});
    // Ctrl+= (no Shift) so the physical "+"/"=" key works without also requiring
    // Shift on layouts where "+" lives on the Shift layer; ImGuiKey_KeypadAdd and
    // ImGuiKey_KeypadSubtract are checked alongside these in editor_panel.cpp for
    // the numpad, since Bind only stores one key per id.
    Bind(ShortcutId::ZoomIn, {ImGuiKey_Equal, true, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::ZoomOut, {ImGuiKey_Minus, true, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::ZoomReset, {ImGuiKey_0, true, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::FormatDocument, {ImGuiKey_F, false, true, true, ShortcutScope::EditorFocused});
    Bind(ShortcutId::QuickFix, {ImGuiKey_Period, true, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::Undo, {ImGuiKey_Z, true, false, false, ShortcutScope::EditorFocused});
    Bind(ShortcutId::Redo, {ImGuiKey_Y, true, false, false, ShortcutScope::EditorFocused});
}

void ShortcutRegistry::Bind(ShortcutId id, ShortcutSpec spec) {
    bindings_[id] = spec;
}

const ShortcutSpec& ShortcutRegistry::Spec(ShortcutId id) const {
    static const ShortcutSpec kEmpty;
    auto it = bindings_.find(id);
    return it != bindings_.end() ? it->second : kEmpty;
}

bool ShortcutRegistry::Pressed(ShortcutId id, bool editor_has_focus) const {
    const ShortcutSpec& spec = Spec(id);
    if (spec.key == ImGuiKey_None) return false;

    if (spec.scope == ShortcutScope::EditorFocused && !editor_has_focus) return false;
    if (spec.scope == ShortcutScope::EditorUnfocused && editor_has_focus) return false;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl != spec.ctrl) return false;
    if (io.KeyShift != spec.shift) return false;
    if (io.KeyAlt != spec.alt) return false;

    return ImGui::IsKeyPressed(spec.key, false);
}

std::string ShortcutRegistry::Label(ShortcutId id) const {
    const ShortcutSpec& spec = Spec(id);
    if (spec.key == ImGuiKey_None) return "";

    std::string label;
    if (spec.ctrl) label += "Ctrl+";
    if (spec.shift) label += "Shift+";
    if (spec.alt) label += "Alt+";
    label += ImGui::GetKeyName(spec.key);
    return label;
}

}
