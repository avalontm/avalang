#pragma once

#include "design/design_document.h"
#include "designer/drop_target.h"
#include "designer/layout_core.h"
#include "designer/node_frame.h"
#include "designer/types.h"

namespace studio::designer {

struct DropResolution {
    NodeId targetId;
    design::DropZone zone = design::DropZone::kInto;
    NodeId anchorId;
    bool targetIsContainer = false;
    DropIndicator indicator;
    bool hasMarker = false;
    LayoutRect marker;
};

bool ResolveDrop(UiNode* root, const NodeId& targetId, const LayoutCore& layout, const NodeFrameMap& frames,
                 const LayoutPoint& canvasPoint, const NodeId& movedId, DropResolution* out);

}
