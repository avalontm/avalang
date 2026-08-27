#include "panels/titlebar_panel.h"

#include <cfloat>
#include <cstdio>
#include <string>
#include <string_view>

#include "imgui.h"

#include "branding/logo_texture.h"
#include "palette.h"
#include "panels/editor_panel.h"
#include "platform/win32_titlebar.h"
#include "plugins/plugin_host.h"
#include "util/i18n.h"
#include "util/settings.h"
#include "util/ui_widgets.h"

namespace studio {

namespace {

constexpr unsigned int kTitleBarBg = 0x0E0E10;
constexpr float kButtonWidth = 46.0f;

ScreenRect ItemScreenRect() {
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    return ScreenRect{p0.x, p0.y, p1.x, p1.y};
}

template <typename DrawIcon>
bool CaptionButton(const char* id, float width, float height, ImU32 hover_bg, DrawIcon draw_icon) {
    ImGui::PushID(id);
    ImGui::InvisibleButton("##btn", ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    if (hovered) {
        dl->AddRectFilled(p0, p1, hover_bg);
    }
    draw_icon(dl, p0, p1);

    ImGui::PopID();
    return clicked;
}

void DrawMinimizeIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 5.0f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    dl->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), col, 1.2f);
}

void DrawMaximizeIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.5f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    dl->AddRect(ImVec2(cx - half, cy - half), ImVec2(cx + half, cy + half), col, 0.0f, 0, 1.2f);
}

void DrawRestoreIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.0f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    const ImU32 bg = palette::U32FromHex(kTitleBarBg);

    dl->AddRect(ImVec2(cx - half + 2.0f, cy - half - 1.0f), ImVec2(cx + half + 2.0f, cy + half - 1.0f), col, 0.0f, 0, 1.1f);
    dl->AddRectFilled(ImVec2(cx - half - 2.0f, cy - half + 1.0f), ImVec2(cx + half - 2.0f, cy + half + 1.0f), bg);
    dl->AddRect(ImVec2(cx - half - 2.0f, cy - half + 1.0f), ImVec2(cx + half - 2.0f, cy + half + 1.0f), col, 0.0f, 0, 1.1f);
}

void DrawCloseIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.5f;
    const ImU32 col = palette::U32FromHex(palette::kTextPrimary);
    dl->AddLine(ImVec2(cx - half, cy - half), ImVec2(cx + half, cy + half), col, 1.2f);
    dl->AddLine(ImVec2(cx - half, cy + half), ImVec2(cx + half, cy - half), col, 1.2f);
}

}

TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized, float height,
                             const std::vector<PluginInfo>& plugins, bool open_extensions_requested) {
    TitleBarResult result;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::FromHex(kTitleBarBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##TitleBar", nullptr, flags);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    constexpr float kIconLeftPad = 14.0f;
    constexpr float kIconSize = 20.0f;
    ImGui::SetCursorPos(ImVec2(kIconLeftPad, (height - kIconSize) * 0.5f));
    {
        const unsigned int logo_texture = branding::GetLogoTextureId();
        if (logo_texture != 0) {
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kIconSize, kIconSize));
        } else {

            const ImVec2 icon_pos = ImGui::GetCursorScreenPos();
            const ImVec2 icon_center(icon_pos.x + kIconSize * 0.5f, icon_pos.y + kIconSize * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(icon_center, kIconSize * 0.5f, palette::U32FromHex(palette::kPrimary));
        }
    }

    // Each menu button is sized to its own translated label (util::AutoButtonSize)
    // instead of a single fixed width tuned for English -- otherwise longer
    // translations (e.g. Spanish) get clipped/overflow past the button box.
    const float kMenuBtnMinWidth = 44.0f;
    const float menu_btn_height = ImGui::GetFrameHeight();

    ImGui::SetCursorPos(ImVec2(kIconLeftPad + kIconSize + 20.0f, (height - menu_btn_height) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, palette::FromHex(palette::kSurface));
    ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kCard));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kBorder));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(palette::kBorder));

    const std::string file_label = util::Tr("menu.file");
    if (ImGui::Button(file_label.c_str(), util::AutoButtonSize(file_label.c_str(), kMenuBtnMinWidth))) {
        ImGui::OpenPopup("##FileMenu");
    }
    result.file_menu_rect = ItemScreenRect();

    ImGui::SetNextWindowPos(ImVec2(result.file_menu_rect.min_x, result.file_menu_rect.max_y + 2.0f));

    ImGui::SetNextWindowSizeConstraints(ImVec2(210.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    static bool open_plugins_modal = false;
    if (open_extensions_requested) {
        open_plugins_modal = true;
    }
    if (ImGui::BeginPopup("##FileMenu")) {
        if (ImGui::MenuItem(util::Tr("menu.file.new_project").c_str())) {
            result.new_project_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.file.new_file").c_str(), "Ctrl+N")) {
            result.new_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.file.open").c_str(), "Ctrl+O")) {
            result.open_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.file.open_folder").c_str())) {
            result.open_folder_requested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(util::Tr("menu.file.save").c_str(), "Ctrl+S")) {
            editor_state.save_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.file.close_tab").c_str(), "Ctrl+W")) {
            editor_state.close_tab_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.file.save_as").c_str(), "Ctrl+Shift+S")) {
            result.save_as_requested = true;
        }
        ImGui::Separator();

        {
            const EditorTab* active = editor_state.Active();
            const bool showing_design = active && active->is_avaui && active->view_mode == TabViewMode::Design;
            const std::string& label = showing_design ? util::Tr("menu.file.view_code") : util::Tr("menu.file.view_design");
            if (ImGui::MenuItem(label.c_str(), "F7")) {
                if (EditorTab* mutable_active = editor_state.Active()) {
                    ToggleTabViewMode(*mutable_active);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(util::Tr("menu.file.preferences").c_str())) {
            if (ImGui::MenuItem(util::Tr("menu.file.settings").c_str(), "Ctrl+,")) {
                result.open_settings_requested = true;
            }
            if (ImGui::MenuItem(util::Tr("menu.file.extensions").c_str())) {
                open_plugins_modal = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(util::Tr("menu.file.exit").c_str(), "Alt+F4")) {
            result.quit_requested = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 4.0f);
    const std::string edit_label = util::Tr("menu.edit");
    if (ImGui::Button(edit_label.c_str(), util::AutoButtonSize(edit_label.c_str(), kMenuBtnMinWidth))) {
        ImGui::OpenPopup("##EditMenu");
    }
    result.edit_menu_rect = ItemScreenRect();
    ImGui::SetNextWindowPos(ImVec2(result.edit_menu_rect.min_x, result.edit_menu_rect.max_y + 2.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginPopup("##EditMenu")) {
        if (ImGui::MenuItem(util::Tr("menu.edit.quick_open").c_str(), "Ctrl+P")) {
            result.quick_open_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.edit.find_in_project").c_str(), "Ctrl+Shift+F")) {
            editor_state.find_in_project_requested = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 4.0f);
    const std::string view_label = util::Tr("menu.view");
    if (ImGui::Button(view_label.c_str(), util::AutoButtonSize(view_label.c_str(), kMenuBtnMinWidth))) {
        ImGui::OpenPopup("##ViewMenu");
    }
    result.view_menu_rect = ItemScreenRect();
    ImGui::SetNextWindowPos(ImVec2(result.view_menu_rect.min_x, result.view_menu_rect.max_y + 2.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginPopup("##ViewMenu")) {
        if (ImGui::MenuItem(util::Tr("menu.view.command_palette").c_str(), "Ctrl+Shift+P")) {
            result.command_palette_requested = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 4.0f);
    const std::string run_label = util::Tr("menu.run");
    if (ImGui::Button(run_label.c_str(), util::AutoButtonSize(run_label.c_str(), kMenuBtnMinWidth))) {
        ImGui::OpenPopup("##RunMenu");
    }
    result.run_menu_rect = ItemScreenRect();
    ImGui::SetNextWindowPos(ImVec2(result.run_menu_rect.min_x, result.run_menu_rect.max_y + 2.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginPopup("##RunMenu")) {
        if (ImGui::MenuItem(util::Tr("menu.run.run_script").c_str(), "F5")) {
            editor_state.run_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.run.run_project").c_str(), "Shift+F5")) {
            editor_state.run_project_requested = true;
        }
        if (ImGui::MenuItem(util::Tr("menu.run.check").c_str(), "Ctrl+Shift+B")) {
            editor_state.check_requested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(util::Tr("menu.run.build").c_str(), "Ctrl+B")) {
            result.build_requested = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 4.0f);
    static bool open_about_modal = false;
    const std::string about_label = util::Tr("menu.about");
    if (ImGui::Button(about_label.c_str(), util::AutoButtonSize(about_label.c_str(), kMenuBtnMinWidth))) {
        open_about_modal = true;
    }
    result.about_rect = ItemScreenRect();

    ImGui::PopStyleColor(4);

    result.any_popup_open = ImGui::IsPopupOpen("##FileMenu") || ImGui::IsPopupOpen("##EditMenu") ||
                             ImGui::IsPopupOpen("##ViewMenu") || ImGui::IsPopupOpen("##RunMenu");

    const std::string about_title = util::Tr("menu.about") + " Ava Studio##AboutModal";
    if (open_about_modal) {
        ImGui::OpenPopup(about_title.c_str());
        open_about_modal = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f));
    if (ImGui::BeginPopupModal(about_title.c_str(), nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        const float window_w = ImGui::GetWindowWidth();

        auto CenteredText = [&](const ImVec4& color, const char* text) {
            const ImVec2 size = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX((window_w - size.x) * 0.5f);
            ImGui::TextColored(color, "%s", text);
        };

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        constexpr float kLogoSize = 56.0f;
        if (const unsigned int logo_texture = branding::GetLogoTextureId(); logo_texture != 0) {
            ImGui::SetCursorPosX((window_w - kLogoSize) * 0.5f);
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kLogoSize, kLogoSize));
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        CenteredText(palette::FromHex(palette::kTextPrimary), "Ava Studio");
        CenteredText(palette::FromHex(palette::kTextMuted), util::Tr("about.tagline").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const std::string version_line = util::Tr("about.version_label") + " 0.1.0";
        CenteredText(palette::FromHex(palette::kTextPrimary), version_line.c_str());
        CenteredText(palette::FromHex(palette::kTextMuted), util::Tr("about.built_on").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const std::string created_by_line = util::Tr("about.created_by") + " AvalonTM";
        CenteredText(palette::FromHex(palette::kTextMuted), created_by_line.c_str());
        {

            const char* kGithubUrl = "https://github.com/avalontm";
            const ImVec2 link_size = ImGui::CalcTextSize(kGithubUrl);
            ImGui::SetCursorPosX((window_w - link_size.x) * 0.5f);
            const ImVec2 link_pos = ImGui::GetCursorScreenPos();
            ImGui::TextColored(palette::FromHex(palette::kPrimary), "%s", kGithubUrl);
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(link_pos.x, link_pos.y + link_size.y),
                    ImVec2(link_pos.x + link_size.x, link_pos.y + link_size.y),
                    palette::U32FromHex(palette::kPrimary));
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                titlebar::OpenUrl(kGithubUrl);
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float close_w = 90.0f;
        ImGui::SetCursorPosX((window_w - close_w) * 0.5f);
        if (ImGui::Button(util::Tr("common.close").c_str(), ImVec2(close_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::EndPopup();
    }

    const std::string plugins_title = util::Tr("plugins.title") + "##PluginsModal";
    if (open_plugins_modal) {
        ImGui::OpenPopup(plugins_title.c_str());
        open_plugins_modal = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));
    if (ImGui::BeginPopupModal(plugins_title.c_str(), nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("plugins.installed").c_str());
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextWrapped("%s", util::Tr("plugins.description").c_str());
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        if (plugins.empty()) {
            ImGui::TextDisabled("%s", util::Tr("plugins.empty").c_str());
        } else {

            ImGui::BeginChild("##PluginsList", ImVec2(0.0f, 220.0f), true);
            for (const PluginInfo& plugin : plugins) {
                bool enabled = plugin.enabled;
                ImGui::PushID(plugin.file_name.c_str());
                if (ImGui::Checkbox(plugin.file_name.c_str(), &enabled)) {

                    result.plugin_toggle_requested = plugin.file_name;
                }
                if (plugin.enabled && !plugin.loaded) {

                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", util::Tr("plugins.not_loaded").c_str());
                }

                std::string meta_line;
                if (!plugin.plugin_name.empty()) meta_line += plugin.plugin_name;
                if (!plugin.version.empty()) meta_line += (meta_line.empty() ? "v" : " v") + plugin.version;
                if (!plugin.author.empty()) meta_line += (meta_line.empty() ? "" : "  ·  ") + util::Tr("plugins.by") + " " + plugin.author;
                if (!meta_line.empty()) {
                    ImGui::Indent();
                    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", meta_line.c_str());
                    ImGui::Unindent();
                }

                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        const float close_button_w = 90.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - close_button_w - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button(util::Tr("common.close").c_str(), ImVec2(close_button_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    {
        const EditorTab* active = editor_state.Active();
        const char* label = (!active || active->file_path.empty()) ? "Ava Studio" : active->file_path.c_str();
        const ImVec2 text_size = ImGui::CalcTextSize(label);
        ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - text_size.x) * 0.5f, (height - text_size.y) * 0.5f));
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    }

    ImGui::SetCursorPos(ImVec2(viewport->WorkSize.x - kButtonWidth * 3.0f, 0.0f));

    const ImU32 neutral_hover = palette::U32FromHex(palette::kCard);
    const ImU32 close_hover = IM_COL32(0xE8, 0x11, 0x23, 0xFF);

    if (CaptionButton("min", kButtonWidth, height, neutral_hover, DrawMinimizeIcon)) {
        result.minimize_clicked = true;
    }
    result.minimize_rect = ItemScreenRect();

    ImGui::SameLine(0.0f, 0.0f);
    if (is_maximized) {
        if (CaptionButton("max", kButtonWidth, height, neutral_hover, DrawRestoreIcon)) {
            result.maximize_or_restore_clicked = true;
        }
    } else {
        if (CaptionButton("max", kButtonWidth, height, neutral_hover, DrawMaximizeIcon)) {
            result.maximize_or_restore_clicked = true;
        }
    }
    result.maximize_rect = ItemScreenRect();

    ImGui::SameLine(0.0f, 0.0f);
    if (CaptionButton("close", kButtonWidth, height, close_hover, DrawCloseIcon)) {
        result.close_clicked = true;
    }
    result.close_rect = ItemScreenRect();

    ImGui::End();
    return result;
}

}
