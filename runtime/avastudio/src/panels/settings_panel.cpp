#include "panels/settings_panel.h"

#include <cstdio>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "plugins/plugin_ui_bridge.h"
#include "util/i18n.h"

namespace studio {

namespace {

int g_selected_plugin_index = -1;

constexpr float kSidebarWidth = 200.0f;

void DrawSidebarGroupHeader(const std::string& label) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label.c_str());
    ImGui::Separator();
}

void DrawGeneralSection(StudioSettings& settings, bool& out_settings_dirty, bool& out_browse_requested,
                         const std::string& browsed_folder) {

    static char modules_path_buf[512] = {};
    static bool seeded = false;
    if (!seeded) {
        std::snprintf(modules_path_buf, sizeof(modules_path_buf), "%s", settings.modules_path.c_str());
        seeded = true;
    }
    if (!browsed_folder.empty()) {

        std::snprintf(modules_path_buf, sizeof(modules_path_buf), "%s", browsed_folder.c_str());
    }

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("settings.language_label").c_str());
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    static const char* kLocaleLabels[] = {"English", "Español"};
    static const util::Locale kLocaleValues[] = {util::Locale::English, util::Locale::Spanish};
    constexpr int kLocaleCount = 2;

    const util::Locale current_locale = util::LocaleFromString(settings.language);
    int current_index = 0;
    for (int i = 0; i < kLocaleCount; ++i) {
        if (kLocaleValues[i] == current_locale) {
            current_index = i;
            break;
        }
    }

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##LanguageCombo", kLocaleLabels[current_index])) {
        for (int i = 0; i < kLocaleCount; ++i) {
            const bool selected = (i == current_index);
            if (ImGui::Selectable(kLocaleLabels[i], selected)) {
                settings.language = util::LocaleToString(kLocaleValues[i]);
                util::SetLocale(kLocaleValues[i]);
                out_settings_dirty = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("settings.modules_folder_label").c_str());
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextWrapped("%s", util::Tr("settings.modules_folder_description").c_str());
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputTextWithHint("##ModulesPathInput", util::Tr("settings.modules_folder_hint").c_str(),
                              modules_path_buf, sizeof(modules_path_buf));
    ImGui::SameLine();
    if (ImGui::Button(util::Tr("common.browse").c_str(), ImVec2(80.0f, 0.0f))) {
        out_browse_requested = true;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ImGui::Button(util::Tr("menu.file.save").c_str(), ImVec2(90.0f, 0.0f))) {
        settings.modules_path = modules_path_buf;
        out_settings_dirty = true;
    }
}

}

void DrawSettingsPanel(StudioSettings& settings, const std::vector<RegisteredPanel>& settings_panels,
                        bool& out_settings_dirty, bool& out_browse_requested, const std::string& browsed_folder,
                        bool* p_open) {
    const std::string title = util::Tr("panel.settings.title") + "###settings";
    ImGui::Begin(title.c_str(), p_open);

    ImGui::BeginChild("##SettingsSidebar", ImVec2(kSidebarWidth, 0.0f), true);
    {
        DrawSidebarGroupHeader(util::Tr("settings.section_general"));
        if (ImGui::Selectable(util::Tr("settings.editor_item").c_str(), g_selected_plugin_index == -1)) {
            g_selected_plugin_index = -1;
        }

        DrawSidebarGroupHeader(util::Tr("plugins.title"));
        if (settings_panels.empty()) {
            ImGui::TextDisabled("%s", util::Tr("settings.no_plugins").c_str());
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

    ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);
    {
        const bool valid_plugin_selection =
            g_selected_plugin_index >= 0 && g_selected_plugin_index < static_cast<int>(settings_panels.size());
        if (!valid_plugin_selection) {
            DrawGeneralSection(settings, out_settings_dirty, out_browse_requested, browsed_folder);
        } else {
            const RegisteredPanel& panel = settings_panels[g_selected_plugin_index];

            AvaPanelContext* ctx = plugins_ui::BeginPanelContext(panel.name.c_str());
            panel.draw(ctx, panel.user_data);
            plugins_ui::EndPanelContext(ctx);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

}
