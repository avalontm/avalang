#pragma once

#include <utility>
#include <vector>

#include "designer/types.h"

namespace studio::designer {

enum class GuideOrientation {
    Horizontal,
    Vertical,
};

struct AlignmentGuide {
    GuideOrientation orientation;
    double position = 0.0;
    double spanStart = 0.0;
    double spanEnd = 0.0;
};

struct SnapResult {
    double dx = 0.0;
    double dy = 0.0;
    std::vector<AlignmentGuide> guides;
};

SnapResult ComputeSnap(const LayoutRect& moving, const std::vector<std::pair<NodeId, LayoutRect>>& candidates,
                        double threshold);

}
