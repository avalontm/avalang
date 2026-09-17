#pragma once

#include <string>

#include "design/live_render_bridge.h"
#include "designer/types.h"

namespace studio::designer {

enum class CanvasMode {
    Design,
    Preview,
};

using PreviewFrame = studio::design::LiveRenderResult;

PreviewFrame BuildPreview(UiComponentTree* tree, const LayoutSize& viewport, const std::string& extends = "",
                           const std::string& projectRoot = "");

bool HasRect(const PreviewFrame& frame, const NodeId& id);
LayoutRect RectOf(const PreviewFrame& frame, const NodeId& id);

}
