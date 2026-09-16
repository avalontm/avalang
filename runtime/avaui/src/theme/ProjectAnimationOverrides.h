#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace avalang {
namespace ui {
namespace theme {

struct AnimationOverride {
    std::optional<double> from;
    std::optional<double> to;
    std::optional<std::string> duration;
    std::optional<std::string> easing;

    void MergeOnto(AnimationOverride& base) const;
};

class ProjectAnimationSheet;

AVA_UI_API ProjectAnimationSheet LoadProjectAnimationOverrides(const std::string& projectRoot);

class AVA_UI_API ProjectAnimationSheet {
public:
    ProjectAnimationSheet() = default;

    AnimationOverride Resolve(const std::string& typeLower, const std::string& trigger) const;

    bool HasAnyAnimations() const { return !targets_.empty(); }

    friend void MergeAnimationFileInto(const std::string& animationFilePath,
                                        ProjectAnimationSheet& sheet);

private:
    std::unordered_map<std::string, AnimationOverride> targets_;
};

}
}
}