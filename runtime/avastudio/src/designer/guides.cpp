#include "designer/guides.h"

#include <algorithm>
#include <cmath>

namespace studio::designer {

namespace {

struct EdgeSet {
    double start;
    double center;
    double end;
};

EdgeSet HorizontalEdges(const LayoutRect& rect) {
    return EdgeSet{rect.x, rect.x + rect.width * 0.5, rect.x + rect.width};
}

EdgeSet VerticalEdges(const LayoutRect& rect) {
    return EdgeSet{rect.y, rect.y + rect.height * 0.5, rect.y + rect.height};
}

}

SnapResult ComputeSnap(const LayoutRect& moving, const std::vector<std::pair<NodeId, LayoutRect>>& candidates,
                        double threshold) {
    SnapResult result;

    const EdgeSet movingH = HorizontalEdges(moving);
    const EdgeSet movingV = VerticalEdges(moving);
    const double movingHValues[3] = {movingH.start, movingH.center, movingH.end};
    const double movingVValues[3] = {movingV.start, movingV.center, movingV.end};

    bool foundDx = false;
    double bestDx = 0.0;
    double bestDxAbs = threshold;
    double bestDxPosition = 0.0;

    bool foundDy = false;
    double bestDy = 0.0;
    double bestDyAbs = threshold;
    double bestDyPosition = 0.0;

    double spanTop = moving.y;
    double spanBottom = moving.y + moving.height;
    double spanLeft = moving.x;
    double spanRight = moving.x + moving.width;

    for (const std::pair<NodeId, LayoutRect>& candidate : candidates) {
        const LayoutRect& rect = candidate.second;
        const EdgeSet h = HorizontalEdges(rect);
        const EdgeSet v = VerticalEdges(rect);
        const double targetHValues[3] = {h.start, h.center, h.end};
        const double targetVValues[3] = {v.start, v.center, v.end};

        for (double mv : movingHValues) {
            for (double tv : targetHValues) {
                const double diff = tv - mv;
                if (std::abs(diff) < bestDxAbs) {
                    bestDxAbs = std::abs(diff);
                    bestDx = diff;
                    bestDxPosition = tv;
                    foundDx = true;
                }
            }
        }

        for (double mv : movingVValues) {
            for (double tv : targetVValues) {
                const double diff = tv - mv;
                if (std::abs(diff) < bestDyAbs) {
                    bestDyAbs = std::abs(diff);
                    bestDy = diff;
                    bestDyPosition = tv;
                    foundDy = true;
                }
            }
        }

        spanTop = std::min(spanTop, rect.y);
        spanBottom = std::max(spanBottom, rect.y + rect.height);
        spanLeft = std::min(spanLeft, rect.x);
        spanRight = std::max(spanRight, rect.x + rect.width);
    }

    if (foundDx) {
        result.dx = bestDx;
        result.guides.push_back(AlignmentGuide{GuideOrientation::Vertical, bestDxPosition, spanTop, spanBottom});
    }
    if (foundDy) {
        result.dy = bestDy;
        result.guides.push_back(AlignmentGuide{GuideOrientation::Horizontal, bestDyPosition, spanLeft, spanRight});
    }

    return result;
}

}
