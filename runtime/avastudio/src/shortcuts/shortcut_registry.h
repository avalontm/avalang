#pragma once

#include <string>
#include <unordered_map>

#include "imgui.h"

namespace studio {

enum class ShortcutScope {
    Global,
    EditorFocused,
    EditorUnfocused,
};

struct ShortcutSpec {
    ImGuiKey key = ImGuiKey_None;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    ShortcutScope scope = ShortcutScope::Global;
};

enum class ShortcutId {
    GotoDefinition,
    Save,
    SaveAs,
    NewFile,
    OpenFile,
    CloseTab,
    Run,
    RunProject,
    Build,
    Check,
    ToggleView,
    FindInProject,
    CommandPalette,
    QuickOpen,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    FormatDocument,
    QuickFix,
};

class ShortcutRegistry {
public:
    static ShortcutRegistry& Instance();

    bool Pressed(ShortcutId id, bool editor_has_focus) const;
    std::string Label(ShortcutId id) const;
    const ShortcutSpec& Spec(ShortcutId id) const;

private:
    ShortcutRegistry();

    void Bind(ShortcutId id, ShortcutSpec spec);

    std::unordered_map<ShortcutId, ShortcutSpec> bindings_;
};

}
