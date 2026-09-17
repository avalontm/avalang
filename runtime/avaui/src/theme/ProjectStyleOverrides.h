#pragma once

#include "Export.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang {
namespace ui {
namespace theme {

struct ControlStyleOverride {
    std::optional<std::string> backgroundColor;
    std::optional<std::string> textColor;
    std::optional<std::string> borderColor;
    std::optional<std::string> fontName;

    std::optional<double> fontSize;
    std::optional<double> borderWidth;
    std::optional<double> borderRadius;
    std::optional<double> padding;
    std::optional<double> margin;
    std::optional<double> spacing;

    void AVA_UI_API MergeOnto(ControlStyleOverride& base) const;
};

struct BreakpointOverride {
    uint32_t minWidthPx = 0;
    ControlStyleOverride global;
    bool hasGlobal = false;
    std::unordered_map<std::string, ControlStyleOverride> perType;
};

class ProjectStyleSheet;

AVA_UI_API ProjectStyleSheet LoadProjectStyleOverrides(const std::string& projectRoot);

class AVA_UI_API ProjectStyleSheet {
public:
    ProjectStyleSheet() = default;

    ControlStyleOverride Resolve(const std::string& typeLower,
                                  bool isPureLayoutContainer = false) const;

    bool HasAnyStyles() const { return hasGlobal_ || !perType_.empty() || !classPerType_.empty(); }

    ControlStyleOverride ResolveState(const std::string& typeLower, const std::string& state) const;

    bool HasAnyStateStyles() const { return !stateGlobal_.empty() || !statePerType_.empty(); }

    ControlStyleOverride ResolveNamed(const std::string& name) const;

    bool HasNamedStyle(const std::string& name) const;

    ControlStyleOverride ResolveClasses(const std::string& typeLower,
                                         const std::vector<std::string>& classesLower) const;

    const std::vector<BreakpointOverride>& Breakpoints() const { return breakpoints_; }

    bool HasAnyResponsiveStyles() const { return !breakpoints_.empty(); }

private:
    friend AVA_UI_API ProjectStyleSheet LoadProjectStyleOverrides(const std::string&);

    friend void MergeStyleFileInto(const std::string& styleFilePath, const std::string& projectRoot,
                                    ProjectStyleSheet& sheet);

    std::unordered_map<std::string, ControlStyleOverride> perType_;
    ControlStyleOverride global_;
    bool hasGlobal_ = false;

    std::unordered_map<std::string, ControlStyleOverride> stateGlobal_;
    std::unordered_map<std::string, ControlStyleOverride> statePerType_;

    std::unordered_map<std::string, ControlStyleOverride> named_;

    std::unordered_map<std::string, ControlStyleOverride> classPerType_;

    std::vector<BreakpointOverride> breakpoints_;
};

}
}
}