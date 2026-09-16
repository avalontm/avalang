#pragma once

#include "theme/ITheme.h"
#include "Export.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang {
namespace ui {
namespace theme {

struct ProjectFontOverride {
    std::string role;
    std::string name;
    std::string filePath;
};

AVA_UI_API std::vector<ProjectFontOverride> LoadProjectFontOverrides(const std::string& projectRoot);

class AVA_UI_API ProjectTheme : public ITheme {
public:
    ProjectTheme(ITheme* base, std::vector<ProjectFontOverride> overrides);

    ThemeColor Color(const std::string& roleName,
                      const ThemeColor& fallback = ThemeColor("000000")) override;
    ThemeFont Font(const std::string& roleName,
                   const ThemeFont& fallback = ThemeFont("Segoe UI", 12)) override;
    ThemeSpacing Spacing() const override;
    std::string Name() const override;
    bool HasColor(const std::string& roleName) const override;
    bool HasFont(const std::string& roleName) const override;
    uint32_t AbiVersion() const override;

    void RegisterProjectFonts() const;

private:
    ITheme* base_;
    std::unordered_map<std::string, ProjectFontOverride> overridesByRole_;
    bool hasDefault_ = false;
    ProjectFontOverride defaultOverride_;
};

}
}
}