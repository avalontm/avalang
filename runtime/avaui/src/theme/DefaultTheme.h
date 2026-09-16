#pragma once

#include "ITheme.h"
#include <unordered_map>
#include <memory>

namespace avalang {
namespace ui {

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

}
}