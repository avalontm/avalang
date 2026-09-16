#pragma once

#include "commands/RenderCommand.h"
#include "Export.h"

#include <vector>

namespace avalang {
namespace ui {

struct DirtyRect {
    float x, y, width, height;
};

struct RenderCommandBounds {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool known = false;
};

AVA_UI_API RenderCommandBounds ComputeCommandBounds(const RenderCommand& cmd);

AVA_UI_API bool RectsIntersect(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
);

AVA_UI_API bool CommandIntersectsAny(const RenderCommand& cmd, const std::vector<DirtyRect>& dirtyRegions);

}
}
