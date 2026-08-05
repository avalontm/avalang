#include "panels/settings_panel.h"

#include <cstdio>

#include "imgui.h"
#include "imgui_stdlib.h" // ImGui::InputText(const char*, std::string*) overload
#include "palette.h"
#include "plugins/plugin_ui_bridge.h"

namespace studio {

namespace {

// Which sidebar section is active -- a plain index scheme rather than a
// string compare per frame: -1 == "General" (the one fixed item),
// >= 0 == index into this frame's `settings_panels` vector. static
// (persists across frames, one Settings panel at a time) same pattern
// as open_properties_modal/open_plugins_modal used to in
// titlebar_panel.cpp before this fase.
int g_selected_plugin_index = -1;

constexpr float kSidebarWidth = 200.0f;

void DrawSidebarGroupHeader(const char* label) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    ImGui::Separator();
}

// "General" section content -- the modules_path field + Browse/Save
// button, moved as-is (same buffer/round-trip mechanics) from the old
// Properties modal (titlebar_panel.cpp, pre-Fase-2/3).
void DrawGeneralSection(StudioSettings& settings, bool& out_settings_dirty, bool& out_browse_requested,
                         const std::string& browsed_folder) {
    // Persistent across frames, same reasoning modules_path_buf had in
    // titlebar_panel.cpp: this panel is always visible (not a modal
    // that reseeds on every open), so the buffer is seeded once here
    // and otherwise only touched by typing or a Browse round-trip.
    static char modules_path_buf[512] = {};
    static bool seeded = false;
    if (!seeded) {
        std::snprintf(modules_path_buf, sizeof(modules_path_buf), "%s", settings.modules_path.c_str());
        seeded = true;
    }
    if (!browsed_folder.empty()) {
        // One frame after "Browse..." succeeded -- see DrawTitleBar's
        // own browsed_folder param for the same round-trip.
        std::snprintf(modules_path_buf, sizeof(modules_path_buf), "%s", browsed_folder.c_str());
    }

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "Modules folder");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextWrapped(
        "In addition to imports relative to the open file, \"import\" also looks here -- "
        "meant for base/shared modules across projects. Leave blank to use the modules "
        "folder next to the executable.");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputTextWithHint("##ModulesPathInput", "(default: folder next to the executable)", modules_path_buf,
                              sizeof(modules_path_buf));
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(80.0f, 0.0f))) {
        out_browse_requested = true;
    }

    // No modal, so a field this "loud" (writes to disk) keeps its own
    // explicit Save button rather than applying on every keystroke --
    // see settings_panel.h's out_settings_dirty comment / the plan's
    // reasoning for keeping this one field's existing button pattern.
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ImGui::Button("Save", ImVec2(90.0f, 0.0f))) {
        settings.modules_path = modules_path_buf;
        out_settings_dirty = true;
    }
}

} // namespace

void DrawSettingsPanel(StudioSettings& settings, const std::vector<RegisteredPanel>& settings_panels,
                        bool& out_settings_dirty, bool& out_browse_requested, const std::string& browsed_folder,
                        bool* p_open) {
    ImGui::Begin("Settings", p_open);

    // Left: sidebar, fixed width, bordered -- same child-window idiom
    // used elsewhere in this codebase for a scrollable side list (see
    // the Plugins modal's "##PluginsList" child in titlebar_panel.cpp).
    ImGui::BeginChild("##SettingsSidebar", ImVec2(kSidebarWidth, 0.0f), true);
    {
        DrawSidebarGroupHeader("General");
        if (ImGui::Selectable("Editor", g_selected_plugin_index == -1)) {
            g_selected_plugin_index = -1;
        }

        DrawSidebarGroupHeader("Plugins");
        if (settings_panels.empty()) {
            ImGui::TextDisabled("(none)");
        } else {
            for (int i = 0; i < static_cast<int>(settings_panels.size()); ++i) {
                const RegisteredPanel& panel = settings_panels[i];
                ImGui::PushID(i);
                const bool is_selected = (g_selected_plugin_index == i);
                if (ImGui::Selectable(panel.name.c_str(), is_selected)) {
                    g_selected_plugin_index = i;
                }
                if (!panel.owner_plugin.empty()) {
                    ImGui::Indent();
                    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", panel.owner_plugin.c_str());
                    ImGui::Unindent();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: content for whichever section is selected. Out-of-range
    // index (e.g. the previously-selected plugin panel's owner got
    // disabled/unloaded between frames) falls back to General instead
    // of drawing nothing -- same "no destructive default" spirit as
    // the rest of this codebase's stale-selection handling.
    ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);
    {
        const bool valid_plugin_selection =
            g_selected_plugin_index >= 0 && g_selected_plugin_index < static_cast<int>(settings_panels.size());
        if (!valid_plugin_selection) {
            DrawGeneralSection(settings, out_settings_dirty, out_browse_requested, browsed_folder);
        } else {
            const RegisteredPanel& panel = settings_panels[g_selected_plugin_index];
            // No Begin/End of our own here -- we're already inside
            // Settings' own window/child, see settings_panel.h's
            // comment on why this differs from the normal plugin-panel
            // draw loop in main.cpp.
            AvaPanelContext* ctx = plugins_ui::BeginPanelContext(panel.name.c_str());
            panel.draw(ctx, panel.user_data);
            plugins_ui::EndPanelContext(ctx);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace studio
