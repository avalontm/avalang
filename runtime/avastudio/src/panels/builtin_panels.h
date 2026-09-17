#pragma once

#include <array>
#include <string_view>

namespace studio {

struct BuiltinPanelInfo {
    std::string_view id;
    std::string_view fallback_label;
    std::string_view tr_key;
};

inline constexpr std::array<BuiltinPanelInfo, 15> kBuiltinPanelNames = {{
    {"Explorer###explorer", "", "panel.explorer.title"},
    {"Toolbox###toolbox", "", "panel.toolbox.title"},
    {"Document Tree###document_tree", "", "panel.document_tree.title"},
    {"Properties###properties", "", "panel.properties.title"},
    {"Theme Tokens###theme_tokens", "", "panel.theme_tokens.title"},
    {"Accessibility###accessibility", "", "panel.accessibility.title"},
    {"Assets###assets", "", "panel.assets.title"},
    {"Animations###animations", "", "panel.animations.title"},
    {"Preview###preview", "Preview", ""},
    {"Terminal###terminal", "", "panel.terminal.title"},
    {"Logs###logs", "", "panel.logs.title"},
    {"Problems###problems", "", "panel.problems.title"},
    {"Find in Project###find_in_project", "", "panel.find_in_project.title"},
    {"Settings###settings", "", "panel.settings.title"},
    {"Build###build", "", "panel.build.title"},
}};

}
