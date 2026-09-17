#include "panels/theme_tokens_panel.h"

#include <memory>

#include "common/ColorParse.h"
#include "designer/property_editor.h"
#include "designer/theme_tokens.h"
#include "imgui.h"
#include "theme/ITheme.h"
#include "theme/ProjectFontOverrides.h"
#include "util/i18n.h"

namespace studio {

namespace {

struct CachedProjectTheme {
    std::string project_root;
    std::unique_ptr<avalang::ui::IThemeProvider> provider;
    std::unique_ptr<avalang::ui::theme::ProjectTheme> theme;
};

CachedProjectTheme g_cached_theme;

avalang::ui::ITheme* GetOrBuildTheme(const std::string& project_root) {
    if (g_cached_theme.theme != nullptr && g_cached_theme.project_root == project_root) {
        return g_cached_theme.theme.get();
    }

    g_cached_theme.project_root = project_root;
    g_cached_theme.provider.reset(avalang::ui::CreateDefaultThemeProvider());
    if (g_cached_theme.provider == nullptr) {
        g_cached_theme.theme.reset();
        return nullptr;
    }

    g_cached_theme.theme = std::make_unique<avalang::ui::theme::ProjectTheme>(
        g_cached_theme.provider->Current(), avalang::ui::theme::LoadProjectFontOverrides(project_root));
    g_cached_theme.theme->RegisterProjectFonts();
    return g_cached_theme.theme.get();
}

void DrawColorTokenRow(avalang::ui::ITheme* theme, const designer::DesignToken& token) {
    const avalang::ui::ThemeColor color = designer::ResolveColorToken(theme, token.name);
    const avalang::ui::Color parsed = avalang::ui::common::ParseColor(color.hex);
    const float rgba[4] = {parsed.r / 255.0f, parsed.g / 255.0f, parsed.b / 255.0f, parsed.a / 255.0f};

    ImGui::PushID(token.name.c_str());
    const float row_start_x = ImGui::GetCursorPosX();
    ImGui::ColorButton("##swatch", ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]),
                        ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(token.name.c_str());
    ImGui::SameLine(row_start_x + 220.0f);
    ImGui::TextDisabled("#%s", color.hex.c_str());
    ImGui::PopID();
}

void DrawFontTokenRow(avalang::ui::ITheme* theme, const designer::DesignToken& token) {
    const avalang::ui::ThemeFont font = designer::ResolveFontToken(theme, token.name);

    ImGui::PushID(token.name.c_str());
    const float row_start_x = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(token.name.c_str());
    ImGui::SameLine(row_start_x + 220.0f);
    ImGui::TextDisabled("%s, %upx, %s", font.name.c_str(), font.sizePoints,
                         font.italic ? "italic" : (font.weight >= 600 ? "bold" : "regular"));
    ImGui::PopID();
}

}

void DrawThemeTokensPanel(const std::string& project_root, bool* p_open) {
    if (!ImGui::Begin(util::Tr("panel.theme_tokens.title").c_str(), p_open)) {
        ImGui::End();
        return;
    }

    avalang::ui::ITheme* theme = GetOrBuildTheme(project_root);
    if (theme == nullptr) {
        ImGui::TextDisabled("%s", util::Tr("panel.theme_tokens.unavailable").c_str());
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("%s", theme->Name().c_str());
    ImGui::Separator();

    if (ImGui::CollapsingHeader(util::Tr("panel.theme_tokens.colors").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const designer::DesignToken& token : designer::KnownDesignTokens()) {
            if (token.kind == designer::DesignTokenKind::Color) {
                DrawColorTokenRow(theme, token);
            }
        }
    }

    if (ImGui::CollapsingHeader(util::Tr("panel.theme_tokens.fonts").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const designer::DesignToken& token : designer::KnownDesignTokens()) {
            if (token.kind == designer::DesignTokenKind::Font) {
                DrawFontTokenRow(theme, token);
            }
        }
    }

    ImGui::End();
}

}
