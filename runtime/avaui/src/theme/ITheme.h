#pragma once

#include "Export.h"
#include <cstdint>
#include <string>
#include <memory>

namespace avalang {
namespace ui {

struct ThemeColor {
    std::string hex;

    ThemeColor() = default;
    explicit ThemeColor(const std::string& h) : hex(h) {}
};

struct ThemeFont {
    std::string name;
    uint32_t sizePoints = 12;
    uint32_t weight = 400;
    bool italic = false;
    std::string filePath;

    ThemeFont() = default;
    ThemeFont(const std::string& n, uint32_t s)
        : name(n), sizePoints(s) {}
    ThemeFont(const std::string& n, uint32_t s, const std::string& file)
        : name(n), sizePoints(s), filePath(file) {}
};

struct ThemeSpacing {
    uint32_t paddingPx = 8;
    uint32_t marginPx = 4;
    uint32_t borderWidthPx = 1;
    uint32_t borderRadiusPx = 4;
    uint32_t containerPaddingPx = 16;
    uint32_t containerGapPx = 12;
};

class ITheme {
public:
    virtual ~ITheme() = default;

    virtual ThemeColor Color(const std::string& roleName,
                            const ThemeColor& fallback = ThemeColor("000000")) = 0;

    virtual ThemeFont Font(const std::string& roleName,
                          const ThemeFont& fallback = ThemeFont("Segoe UI", 12)) = 0;

    virtual ThemeSpacing Spacing() const = 0;

    virtual std::string Name() const = 0;

    virtual bool HasColor(const std::string& roleName) const = 0;

    virtual bool HasFont(const std::string& roleName) const = 0;

    virtual uint32_t AbiVersion() const = 0;
};

class IThemeProvider {
public:
    virtual ~IThemeProvider() = default;

    virtual ITheme* Current() = 0;

    virtual bool SetTheme(const std::string& themeName) = 0;

    virtual bool Register(std::unique_ptr<ITheme> theme, const std::string& name) = 0;

    virtual uint32_t AbiVersion() const = 0;
};

AVA_UI_API IThemeProvider* CreateDefaultThemeProvider();

}
}