#include "designer/drop_target.h"

#include <algorithm>
#include <cctype>

namespace studio::designer {

namespace {

constexpr double kBandFraction = 0.2;
constexpr double kMinBandPx = 6.0;
constexpr double kMaxBandPx = 16.0;
constexpr double kMaxBandShare = 0.3;

double SiblingBandHeight(double targetHeight) {
    const double band = std::clamp(targetHeight * kBandFraction, kMinBandPx, kMaxBandPx);
    return std::min(band, targetHeight * kMaxBandShare);
}

}

studio::design::DropZone ComputeDropZone(const LayoutRect& targetRect, const LayoutPoint& point, bool isContainer,
                                         bool allowSibling) {
    const double height = std::max(targetRect.height, 1.0);
    const double offset = point.y - targetRect.y;

    if (!isContainer) {
        return offset / height <= 0.5 ? studio::design::DropZone::kBefore : studio::design::DropZone::kAfter;
    }
    if (!allowSibling) {
        return studio::design::DropZone::kInto;
    }

    const double band = SiblingBandHeight(height);
    if (offset < band) return studio::design::DropZone::kBefore;
    if (offset > height - band) return studio::design::DropZone::kAfter;
    return studio::design::DropZone::kInto;
}

DropIndicator ComputeDropIndicator(const LayoutRect& targetRect, studio::design::DropZone zone, bool isContainer) {
    DropIndicator indicator;

    if (isContainer && zone == studio::design::DropZone::kInto) {
        indicator.rect = targetRect;
        indicator.isLine = false;
        return indicator;
    }

    constexpr double kLineThickness = 3.0;
    indicator.isLine = true;
    const double lineY = (zone == studio::design::DropZone::kBefore)
                              ? targetRect.y - kLineThickness * 0.5
                              : targetRect.y + targetRect.height - kLineThickness * 0.5;
    indicator.rect = LayoutRect{targetRect.x, lineY, targetRect.width, kLineThickness};
    return indicator;
}

namespace {

constexpr double kMarkerThickness = 3.0;

double CenterX(const LayoutRect& rect) { return rect.x + rect.width * 0.5; }
double CenterY(const LayoutRect& rect) { return rect.y + rect.height * 0.5; }

std::string LowerAscii(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

size_t IndexAlongAxis(const std::vector<LayoutRect>& rects, double coordinate, bool horizontal) {
    for (size_t i = 0; i < rects.size(); ++i) {
        const double center = horizontal ? CenterX(rects[i]) : CenterY(rects[i]);
        if (coordinate < center) return i;
    }
    return rects.size();
}

size_t IndexInGrid(const std::vector<LayoutRect>& rects, size_t columns, const LayoutPoint& point) {
    const size_t count = rects.size();
    for (size_t rowStart = 0; rowStart < count; rowStart += columns) {
        const size_t rowEnd = std::min(rowStart + columns, count);
        double rowBottom = rects[rowStart].y + rects[rowStart].height;
        for (size_t i = rowStart + 1; i < rowEnd; ++i) {
            rowBottom = std::max(rowBottom, rects[i].y + rects[i].height);
        }
        if (point.y > rowBottom) continue;
        for (size_t i = rowStart; i < rowEnd; ++i) {
            if (point.x < CenterX(rects[i])) return i;
        }
        return rowEnd;
    }
    return count;
}

LayoutRect VerticalMarker(const std::vector<LayoutRect>& rects, size_t index) {
    double left = rects.front().x;
    double right = rects.front().x + rects.front().width;
    for (const LayoutRect& rect : rects) {
        left = std::min(left, rect.x);
        right = std::max(right, rect.x + rect.width);
    }
    const double edge = index < rects.size() ? rects[index].y : rects.back().y + rects.back().height;
    return LayoutRect{left, edge - kMarkerThickness * 0.5, right - left, kMarkerThickness};
}

LayoutRect HorizontalMarker(const std::vector<LayoutRect>& rects, size_t index) {
    double top = rects.front().y;
    double bottom = rects.front().y + rects.front().height;
    for (const LayoutRect& rect : rects) {
        top = std::min(top, rect.y);
        bottom = std::max(bottom, rect.y + rect.height);
    }
    const double edge = index < rects.size() ? rects[index].x : rects.back().x + rects.back().width;
    return LayoutRect{edge - kMarkerThickness * 0.5, top, kMarkerThickness, bottom - top};
}

LayoutRect GridMarker(const std::vector<LayoutRect>& rects, size_t columns, size_t index) {
    const bool afterPrevious = index >= rects.size() || (index > 0 && index % columns == 0);
    const LayoutRect& anchor = afterPrevious ? rects[std::min(index, rects.size()) - 1] : rects[index];
    const double edge = afterPrevious ? anchor.x + anchor.width : anchor.x;
    return LayoutRect{edge - kMarkerThickness * 0.5, anchor.y, kMarkerThickness, anchor.height};
}

}

FlowLayout FlowLayoutOf(const std::string& typeName, bool horizontalDirection, int gridColumns) {
    const std::string type = LowerAscii(typeName);
    FlowLayout flow;
    if (type == "row") {
        flow.axis = FlowAxis::kHorizontal;
    } else if (type == "column" || type == "for" || type == "if") {
        flow.axis = FlowAxis::kVertical;
    } else if (type == "scrollview" || type == "listview" || type == "flex") {
        flow.axis = horizontalDirection ? FlowAxis::kHorizontal : FlowAxis::kVertical;
    } else if (type == "grid") {
        flow.columns = std::max(1, gridColumns);
        flow.axis = flow.columns > 1 ? FlowAxis::kGrid : FlowAxis::kVertical;
    }
    return flow;
}

size_t ComputeInsertIndex(const FlowLayout& flow, const std::vector<LayoutRect>& childRects, const LayoutPoint& point) {
    switch (flow.axis) {
        case FlowAxis::kVertical: return IndexAlongAxis(childRects, point.y, false);
        case FlowAxis::kHorizontal: return IndexAlongAxis(childRects, point.x, true);
        case FlowAxis::kGrid: return IndexInGrid(childRects, static_cast<size_t>(std::max(1, flow.columns)), point);
        case FlowAxis::kNone: break;
    }
    return childRects.size();
}

size_t ResolveInsertPosition(const std::vector<std::string>& childIds, size_t index, const std::string& movedId) {
    size_t position = index;
    while (position < childIds.size() && childIds[position] == movedId) ++position;
    return position;
}

bool ComputeInsertMarker(const FlowLayout& flow, const std::vector<LayoutRect>& childRects, size_t index,
                         LayoutRect& out) {
    if (childRects.empty()) return false;
    switch (flow.axis) {
        case FlowAxis::kVertical: out = VerticalMarker(childRects, index); return true;
        case FlowAxis::kHorizontal: out = HorizontalMarker(childRects, index); return true;
        case FlowAxis::kGrid:
            out = GridMarker(childRects, static_cast<size_t>(std::max(1, flow.columns)), index);
            return true;
        case FlowAxis::kNone: break;
    }
    return false;
}

}
