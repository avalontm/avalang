#pragma once

#include <array>
#include <string_view>

namespace studio {

// Every built-in panel the View menu offers a show/hide checkbox for --
// same idea as VSCode's View menu listing Explorer, Search, etc. with a
// checkmark for each. Deliberately NOT included here:
//  - "Code Editor": the main editing surface, not something VSCode-style
//    apps let you hide entirely (there's nothing useful behind it).
//  - "Toolbox": only ever drawn while a .avaui tab is showing Design view
//    (see main.cpp) -- it isn't independently open/closed the way these
//    always-tabbed panels are.
// Both this array and main.cpp's `panel_open` map (which stores the
// actual runtime open/closed flag per name) key off these exact strings,
// which must also match the literal passed to each panel's
// ImGui::Begin() call -- see explorer_panel.cpp, properties_panel.cpp,
// preview_panel.cpp, terminal_panel.cpp, logs_panel.cpp.
inline constexpr std::array<std::string_view, 5> kBuiltinPanelNames = {
    "Explorer", "Properties", "Preview", "Terminal", "Output",
};

} // namespace studio
