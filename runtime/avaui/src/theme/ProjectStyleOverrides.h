#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang::ui::theme {

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

    void MergeOnto(ControlStyleOverride& base) const;
};

class ProjectStyleSheet;

AVA_UI_API ProjectStyleSheet LoadProjectStyleOverrides(const std::string& projectRoot);

class AVA_UI_API ProjectStyleSheet {
public:
    ProjectStyleSheet() = default;

    // isPureLayoutContainer: pass true for components that are pure layout
    // wrappers (row/column/stack/hstack/vstack/flex) rather than visible
    // surfaces (container/dialog/...). Pure layout wrappers have no card of
    // their own, so a `style *` reset that sets `margin`/`backgroundColor`
    // for every component would otherwise silently give them a box that
    // doesn't line up with the content they wrap. For those types, the
    // global `style *` values for margin/backgroundColor are ignored unless
    // a `style row`/`style column`/etc. block explicitly sets them.
    ControlStyleOverride Resolve(const std::string& typeLower,
                                  bool isPureLayoutContainer = false) const;

    bool HasAnyStyles() const { return hasGlobal_ || !perType_.empty() || !classPerType_.empty(); }

    ControlStyleOverride ResolveState(const std::string& typeLower, const std::string& state) const;

    bool HasAnyStateStyles() const { return !stateGlobal_.empty() || !statePerType_.empty(); }

    ControlStyleOverride ResolveNamed(const std::string& name) const;

    bool HasNamedStyle(const std::string& name) const;

    ControlStyleOverride ResolveClasses(const std::string& typeLower,
                                         const std::vector<std::string>& classesLower) const;

private:
    friend ProjectStyleSheet LoadProjectStyleOverrides(const std::string&);

    friend void MergeStyleFileInto(const std::string& styleFilePath, const std::string& projectRoot,
                                    ProjectStyleSheet& sheet);

    std::unordered_map<std::string, ControlStyleOverride> perType_;
    ControlStyleOverride global_;
    bool hasGlobal_ = false;

    std::unordered_map<std::string, ControlStyleOverride> stateGlobal_;
    std::unordered_map<std::string, ControlStyleOverride> statePerType_;

    std::unordered_map<std::string, ControlStyleOverride> named_;

    std::unordered_map<std::string, ControlStyleOverride> classPerType_;
};

} // namespace avalang::ui::theme
