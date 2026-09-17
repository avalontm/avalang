#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "designer/types.h"
#include "layout/LayoutEngine.h"

namespace studio::designer {

class LayoutCore {
public:
    LayoutCore();

    LayoutRect Compute(UiComponentTree* tree, const LayoutRect& available);
    LayoutRect ComputeFromPhysicalPixels(UiComponentTree* tree, const LayoutRect& physicalAvailable);

    LayoutSize MeasureNode(UiNode* node, const LayoutConstraints& constraints);
    LayoutRect ArrangeNode(UiComponentTree* tree, UiNode* node, const LayoutRect& finalRect);

    void SetTextEvaluator(std::function<std::string(const std::string&)> eval);

    bool HasRect(const NodeId& id) const;
    LayoutRect RectOf(const NodeId& id) const;
    const std::unordered_map<NodeId, LayoutRect>& Rects() const { return rects_; }

    void AdoptRects(const std::unordered_map<NodeId, LayoutRect>& rects);

    void Clear();

private:
    void CacheRects(UiComponentTree* tree, avalang::ui::ILayoutNode* layoutNode);

    std::unique_ptr<avalang::ui::LayoutEngine> engine_;
    std::unordered_map<NodeId, LayoutRect> rects_;
};

}
