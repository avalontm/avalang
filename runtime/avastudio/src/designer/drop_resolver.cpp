#include "designer/drop_resolver.h"

#include <algorithm>
#include <string>
#include <vector>

#include "designer/node_kind.h"
#include "layout/LayoutProperties.h"

namespace studio::designer {

namespace {

FlowLayout FlowLayoutOfNode(const UiNode* node) {
    bool horizontal = false;
    if (const PropertyValue* direction = node->GetProperty("direction")) {
        horizontal = direction->Type() == PropertyType::String && direction->AsString() == "horizontal";
    }
    const int columns = static_cast<int>(avalang::ui::layout::ReadNumber(node, "columns", 1.0));
    return FlowLayoutOf(node->TypeName(), horizontal, columns);
}

}

bool ResolveDrop(UiNode* root, const NodeId& targetId, const LayoutCore& layout, const NodeFrameMap& frames,
                 const LayoutPoint& canvasPoint, const NodeId& movedId, DropResolution* out) {
    if (out == nullptr || root == nullptr) return false;
    UiNode* target = design::FindNodeById(root, targetId);
    if (target == nullptr) return false;
    const auto frameIt = frames.find(targetId);
    if (frameIt == frames.end()) return false;

    const LayoutRect& content = frameIt->second.content;
    const bool isContainer = IsContainerType(target->TypeName());
    const bool allowSibling = IdOf(root) != targetId;

    DropResolution resolution;
    resolution.targetId = targetId;
    resolution.targetIsContainer = isContainer;

    const LayoutRect zoneRect{content.x, content.y, content.width, std::max(content.height, 1.0)};
    resolution.zone = ComputeDropZone(zoneRect, LayoutPoint{content.x, canvasPoint.y}, isContainer, allowSibling);
    resolution.indicator = ComputeDropIndicator(content, resolution.zone, isContainer);

    if (resolution.zone == design::DropZone::kInto) {
        std::vector<NodeId> childIds;
        std::vector<LayoutRect> childRects;
        for (UiNode* child : target->Children()) {
            if (IsDialogNode(child)) continue;
            const NodeId childId = IdOf(child);
            if (!layout.HasRect(childId)) continue;
            childIds.push_back(childId);
            childRects.push_back(layout.RectOf(childId));
        }

        if (!childRects.empty()) {
            const FlowLayout flow = FlowLayoutOfNode(target);
            const size_t index = ComputeInsertIndex(flow, childRects, canvasPoint);
            const size_t position = ResolveInsertPosition(childIds, index, movedId);
            if (position < childIds.size()) resolution.anchorId = childIds[position];
            resolution.hasMarker = ComputeInsertMarker(flow, childRects, position, resolution.marker);
        }
    }

    *out = resolution;
    return true;
}

}
