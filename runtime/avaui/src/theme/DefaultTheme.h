#pragma once

#include "ITheme.h"
#include <unordered_map>
#include <memory>

namespace avalang::ui {

/**
 * Default light theme (Windows Fluent-inspired palette).
 * 
 * Color palette:
 *   Primary: #0078D4 (Windows blue)
 *   Background: #FFFFFF, #F3F3F3 (surface)
 *   Text: #333333, #767676 (secondary)
 *   Borders: #CCCCCC, #E0E0E0 (light)
 *   Semantic: #107C10 (success), #D83B01 (error), #FFB900 (warning)
 * 
 * Typography:
 *   Heading1: Segoe UI, 28pt, Bold
 *   Heading2: Segoe UI, 20pt, Bold
 *   Body: Segoe UI, 12pt, Normal
 *   Caption: Segoe UI, 11pt, Normal
 *   Button: Segoe UI, 12pt, Normal
 * 
 * Spacing:
 *   Padding: 8px, Margin: 4px, BorderWidth: 1px, BorderRadius: 4px
 */
class DefaultTheme : public ITheme {
public:
    DefaultTheme();

    ThemeColor Color(const std::string& roleName,
                    const ThemeColor& fallback = ThemeColor("000000")) override;
    ThemeFont Font(const std::string& roleName,
                  const ThemeFont& fallback = ThemeFont("Segoe UI", 12)) override;
    ThemeSpacing Spacing() const override;
    std::string Name() const override { return "Default Light"; }
    bool HasColor(const std::string& roleName) const override;
    bool HasFont(const std::string& roleName) const override;
    uint32_t AbiVersion() const override { return 16; }

private:
    std::unordered_map<std::string, ThemeColor> colors_;
    std::unordered_map<std::string, ThemeFont> fonts_;
    ThemeSpacing spacing_;

    void InitColors();
    void InitFonts();
};

/**
 * Theme provider: holds multiple themes, switches between them.
 */
class ThemeProvider : public IThemeProvider {
public:
    ThemeProvider();

    ITheme* Current() override;
    bool SetTheme(const std::string& themeName) override;
    bool Register(std::unique_ptr<ITheme> theme, const std::string& name) override;
    uint32_t AbiVersion() const override { return 16; }

private:
    std::unordered_map<std::string, std::unique_ptr<ITheme>> themes_;
    ITheme* current_;
};

} // namespace avalang::ui
