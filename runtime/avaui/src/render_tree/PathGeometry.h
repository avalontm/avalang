#pragma once

#include <cstdint>
#include <vector>

namespace avalang {
namespace ui {
namespace render {

enum class PathSegmentType : std::uint8_t {
    MoveTo,
    LineTo,
    CubicCurveTo,
    Close,
};

struct PathSegment {
    PathSegmentType type = PathSegmentType::MoveTo;
    float x = 0.0f;
    float y = 0.0f;
    float cx1 = 0.0f;
    float cy1 = 0.0f;
    float cx2 = 0.0f;
    float cy2 = 0.0f;
};

using PathData = std::vector<PathSegment>;

}
}
}
