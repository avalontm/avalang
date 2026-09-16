#include "theme/DefaultTheme.h"

namespace avalang {
namespace ui {

DefaultTheme::DefaultTheme() {
    InitColors();
    InitFonts();

    spacing_.paddingPx = 8;
    spacing_.marginPx = 4;
    spacing_.borderWidthPx = 1;
    spacing_.borderRadiusPx = 4;
    spacing_.containerPaddingPx = 16;
    spacing_.containerGapPx = 12;
}

void DefaultTheme::InitColors() {
    colors_["primary"] = ThemeColor("0078D4");
    colors_["primaryLight"] = ThemeColor("2B88D9");
    colors_["primaryDark"] = ThemeColor("005A9E");
    colors_["primaryAlt"] = ThemeColor("4B8AC8");

    colors_["secondary"] = ThemeColor("605E57");

    colors_["background"] = ThemeColor("FFFFFF");
    colors_["surface"] = ThemeColor("F3F3F3");
    colors_["surfaceVariant"] = ThemeColor("E8E8E8");

    colors_["text"] = ThemeColor("333333");
    colors_["textSecondary"] = ThemeColor("767676");
    colors_["textDisabled"] = ThemeColor("A19F9D");
    colors_["textInverse"] = ThemeColor("FFFFFF");

    colors_["border"] = ThemeColor("CCCCCC");
    colors_["borderLight"] = ThemeColor("E0E0E0");
    colors_["borderDark"] = ThemeColor("999999");

    colors_["success"] = ThemeColor("107C10");
    colors_["error"] = ThemeColor("D83B01");
    colors_["warning"] = ThemeColor("FFB900");
    colors_["info"] = ThemeColor("0078D4");

    colors_["buttonPrimary"] = ThemeColor("0078D4");
    colors_["buttonPrimaryHover"] = ThemeColor("2B88D9");
    colors_["buttonPrimaryActive"] = ThemeColor("005A9E");
    colors_["buttonPrimaryDisabled"] = ThemeColor("CCCCCC");

    colors_["buttonSecondary"] = ThemeColor("F3F3F3");
    colors_["buttonSecondaryHover"] = ThemeColor("E8E8E8");
    colors_["buttonSecondaryActive"] = ThemeColor("D8D8D8");
    colors_["buttonSecondaryDisabled"] = ThemeColor("F9F9F9");

    colors_["inputBackground"] = ThemeColor("FFFFFF");
    colors_["inputBorder"] = ThemeColor("A4A4A4");
    colors_["inputBorderActive"] = ThemeColor("0078D4");
    colors_["inputBorderError"] = ThemeColor("D83B01");

    colors_["linkDefault"] = ThemeColor("0078D4");
    colors_["linkVisited"] = ThemeColor("4B3E8F");
}

void DefaultTheme::InitFonts() {
    fonts_["heading1"] = ThemeFont("Segoe UI", 28);
    fonts_["heading1"].weight = 700;

    fonts_["heading2"] = ThemeFont("Segoe UI", 20);
    fonts_["heading2"].weight = 700;

    fonts_["heading3"] = ThemeFont("Segoe UI", 16);
    fonts_["heading3"].weight = 700;

    fonts_["body"] = ThemeFont("Segoe UI", 12);
    fonts_["body"].weight = 400;

    fonts_["bodySmall"] = ThemeFont("Segoe UI", 11);
    fonts_["bodySmall"].weight = 400;

    fonts_["bodySemibold"] = ThemeFont("Segoe UI", 12);
    fonts_["bodySemibold"].weight = 600;

    fonts_["caption"] = ThemeFont("Segoe UI", 11);
    fonts_["caption"].weight = 400;

    fonts_["label"] = ThemeFont("Segoe UI", 12);
    fonts_["label"].weight = 600;

    fonts_["button"] = ThemeFont("Segoe UI", 12);
    fonts_["button"].weight = 600;

    fonts_["buttonSmall"] = ThemeFont("Segoe UI", 11);
    fonts_["buttonSmall"].weight = 600;

    fonts_["link"] = ThemeFont("Segoe UI", 12);
    fonts_["link"].weight = 400;

    fonts_["subtitle"] = ThemeFont("Segoe UI", 14);
    fonts_["subtitle"].weight = 400;
}

ThemeColor DefaultTheme::Color(const std::string& roleName,
                               const ThemeColor& fallback) {
    auto it = colors_.find(roleName);
    if (it != colors_.end()) {
        return it->second;
    }
    return fallback;
}

ThemeFont DefaultTheme::Font(const std::string& roleName,
                            const ThemeFont& fallback) {
    auto it = fonts_.find(roleName);
    if (it != fonts_.end()) {
        return it->second;
    }
    return fallback;
}

ThemeSpacing DefaultTheme::Spacing() const {
    return spacing_;
}

bool DefaultTheme::HasColor(const std::string& roleName) const {
    return colors_.find(roleName) != colors_.end();
}

bool DefaultTheme::HasFont(const std::string& roleName) const {
    return fonts_.find(roleName) != fonts_.end();
}

ThemeProvider::ThemeProvider() : current_(nullptr) {
    auto defaultTheme = std::make_unique<DefaultTheme>();
    current_ = defaultTheme.get();
    themes_["Default Light"] = std::move(defaultTheme);
}

ITheme* ThemeProvider::Current() {
    return current_;
}

bool ThemeProvider::SetTheme(const std::string& themeName) {
    auto it = themes_.find(themeName);
    if (it != themes_.end()) {
        current_ = it->second.get();
        return true;
    }
    return false;
}

bool ThemeProvider::Register(std::unique_ptr<ITheme> theme,
                            const std::string& name) {
    if (!theme || name.empty()) {
        return false;
    }
    themes_[name] = std::move(theme);
    if (current_ == nullptr) {
        current_ = themes_[name].get();
    }
    return true;
}

IThemeProvider* CreateDefaultThemeProvider() {
    return new ThemeProvider();
}

}
}