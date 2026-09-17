#pragma once

#include <string>
#include <vector>

#include "theme/ITheme.h"

namespace studio::designer {

enum class DesignTokenKind {
    Color,
    Font,
};

struct DesignToken {
    std::string name;
    DesignTokenKind kind;
};

const std::vector<DesignToken>& KnownDesignTokens();

avalang::ui::ThemeColor ResolveColorToken(avalang::ui::ITheme* theme, const std::string& name);
avalang::ui::ThemeFont ResolveFontToken(avalang::ui::ITheme* theme, const std::string& name);

double ContrastRatio(const avalang::ui::ThemeColor& a, const avalang::ui::ThemeColor& b);

}
