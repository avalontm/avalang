#include "commands/RenderCommandBounds.h"
#include "layout/TextMeasure.h"

#include <algorithm>
#include <cstring>

namespace avalang {
namespace ui {

namespace {

RenderCommandBounds MakeBounds(float x, float y, float w, float h) {
    RenderCommandBounds bounds;
    bounds.x = w >= 0.0f ? x : x + w;
    bounds.y = h >= 0.0f ? y : y + h;
    bounds.width = std::abs(w);
    bounds.height = std::abs(h);
    bounds.known = true;
    return bounds;
}

RenderCommandBounds TextBounds(const RenderCommand& cmd) {
    const auto& t = cmd.drawText;
    if (!t.text) {
        return RenderCommandBounds{};
    }

    std::string fontName = t.fontName ? t.fontName : "";
    double width = t.maxWidth > 0.0f
        ? static_cast<double>(t.maxWidth)
        : layout::EstimateTextWidth(t.text, t.fontSize, fontName);
    double height = t.wrap
        ? layout::WrappedLineHeight(t.fontSize, fontName)
        : layout::DefaultLineHeight(t.fontSize, fontName);

    return MakeBounds(t.x, t.y, static_cast<float>(width), static_cast<float>(height));
}

}

RenderCommandBounds ComputeCommandBounds(const RenderCommand& cmd) {
    switch (cmd.type) {
        case RenderCommandType::DrawRectangle:
            return MakeBounds(cmd.drawRect.x, cmd.drawRect.y, cmd.drawRect.width, cmd.drawRect.height);

        case RenderCommandType::DrawEllipse:
            return MakeBounds(
                cmd.drawEllipse.cx - cmd.drawEllipse.rx,
                cmd.drawEllipse.cy - cmd.drawEllipse.ry,
                cmd.drawEllipse.rx * 2.0f,
                cmd.drawEllipse.ry * 2.0f
            );

        case RenderCommandType::DrawImage:
            return MakeBounds(cmd.drawImage.x, cmd.drawImage.y, cmd.drawImage.width, cmd.drawImage.height);

        case RenderCommandType::DrawButton:
            return MakeBounds(cmd.drawButton.x, cmd.drawButton.y, cmd.drawButton.width, cmd.drawButton.height);

        case RenderCommandType::DrawText:
            return TextBounds(cmd);

        case RenderCommandType::DrawLink: {
            const auto& l = cmd.drawLink;
            if (!l.text) return RenderCommandBounds{};
            std::string fontName = l.fontName ? l.fontName : "";
            double width = layout::EstimateTextWidth(l.text, l.fontSize, fontName);
            double height = layout::DefaultLineHeight(l.fontSize, fontName);
            return MakeBounds(l.x, l.y, static_cast<float>(width), static_cast<float>(height));
        }

        case RenderCommandType::DrawPath: {
            const auto& p = cmd.drawPath;
            if (p.segments.empty()) return RenderCommandBounds{};

            float minX = p.x, minY = p.y, maxX = p.x, maxY = p.y;
            for (const auto& seg : p.segments) {
                minX = std::min({minX, seg.x, seg.cx1, seg.cx2});
                minY = std::min({minY, seg.y, seg.cy1, seg.cy2});
                maxX = std::max({maxX, seg.x, seg.cx1, seg.cx2});
                maxY = std::max({maxY, seg.y, seg.cy1, seg.cy2});
            }
            return MakeBounds(minX, minY, maxX - minX, maxY - minY);
        }

        // Structural commands (clip/transform stack) and free-form HTML have no
        // well-defined draw-space bounds; report unknown so they are never culled.
        case RenderCommandType::PushClip:
        case RenderCommandType::PopClip:
        case RenderCommandType::Translate:
        case RenderCommandType::Scale:
        case RenderCommandType::Rotate:
        case RenderCommandType::DrawHtmlFragment:
        default:
            return RenderCommandBounds{};
    }
}

bool RectsIntersect(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
) {
    if (aw <= 0.0f || ah <= 0.0f || bw <= 0.0f || bh <= 0.0f) {
        return false;
    }
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

bool CommandIntersectsAny(const RenderCommand& cmd, const std::vector<DirtyRect>& dirtyRegions) {
    if (dirtyRegions.empty()) {
        return false;
    }

    RenderCommandBounds bounds = ComputeCommandBounds(cmd);
    if (!bounds.known) {
        return true;
    }

    for (const DirtyRect& rect : dirtyRegions) {
        if (RectsIntersect(bounds.x, bounds.y, bounds.width, bounds.height,
                            rect.x, rect.y, rect.width, rect.height)) {
            return true;
        }
    }
    return false;
}

}
}
