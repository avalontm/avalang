#include "designer/theme_tokens.h"

#include <algorithm>
#include <cmath>

#include "common/ColorParse.h"

namespace studio::designer {

namespace {

std::vector<DesignToken> BuildKnownDesignTokens() {
    std::vector<DesignToken> tokens;

    for (const char* name : {
             "primary", "primaryLight", "primaryDark", "primaryAlt", "secondary",
             "background", "surface", "surfaceVariant",
             "text", "textSecondary", "textDisabled", "textInverse",
             "border", "borderLight", "borderDark",
             "success", "error", "warning", "info",
             "buttonPrimary", "buttonPrimaryHover", "buttonPrimaryActive", "buttonPrimaryDisabled",
             "buttonSecondary", "buttonSecondaryHover", "buttonSecondaryActive", "buttonSecondaryDisabled",
             "inputBackground", "inputBorder", "inputBorderActive", "inputBorderError",
             "linkDefault", "linkVisited",
         }) {
        tokens.push_back({name, DesignTokenKind::Color});
    }

    for (const char* name : {
             "heading1", "heading2", "heading3",
             "body", "bodySmall", "bodySemibold",
             "caption", "label", "button", "buttonSmall", "link", "subtitle",
         }) {
        tokens.push_back({name, DesignTokenKind::Font});
    }

    return tokens;
}

double LinearizeChannel(std::uint8_t channel) {
    const double c = static_cast<double>(channel) / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double RelativeLuminance(const avalang::ui::Color& color) {
    return 0.2126 * LinearizeChannel(color.r) + 0.7152 * LinearizeChannel(color.g) +
           0.0722 * LinearizeChannel(color.b);
}

}

const std::vector<DesignToken>& KnownDesignTokens() {
    static const std::vector<DesignToken> tokens = BuildKnownDesignTokens();
    return tokens;
}

avalang::ui::ThemeColor ResolveColorToken(avalang::ui::ITheme* theme, const std::string& name) {
    if (!theme) {
        return avalang::ui::ThemeColor("000000");
    }
    return theme->Color(name);
}

avalang::ui::ThemeFont ResolveFontToken(avalang::ui::ITheme* theme, const std::string& name) {
    if (!theme) {
        return avalang::ui::ThemeFont("Segoe UI", 12);
    }
    return theme->Font(name);
}

double ContrastRatio(const avalang::ui::ThemeColor& a, const avalang::ui::ThemeColor& b) {
    const avalang::ui::Color colorA = avalang::ui::common::ParseColor(a.hex);
    const avalang::ui::Color colorB = avalang::ui::common::ParseColor(b.hex);

    const double lumA = RelativeLuminance(colorA);
    const double lumB = RelativeLuminance(colorB);

    const double lighter = std::max(lumA, lumB);
    const double darker = std::min(lumA, lumB);

    return (lighter + 0.05) / (darker + 0.05);
}

}
