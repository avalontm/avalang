#include "designer/layout_core.h"

namespace studio::designer {

LayoutCore::LayoutCore() : engine_(avalang::ui::LayoutEngine::Create()) {}

LayoutRect LayoutCore::Compute(UiComponentTree* tree, const LayoutRect& available) {
    if (!tree || !tree->Root()) {
        return LayoutRect{};
    }

    avalang::ui::ILayoutNode* root = engine_->Compute(tree->Root(), available);
    CacheRects(tree, root);
    return root ? root->Rect() : LayoutRect{};
}

LayoutRect LayoutCore::ComputeFromPhysicalPixels(UiComponentTree* tree, const LayoutRect& physicalAvailable) {
    if (!tree || !tree->Root()) {
        return LayoutRect{};
    }

    avalang::ui::ILayoutNode* root = engine_->ComputeFromPhysicalPixels(tree->Root(), physicalAvailable);
    CacheRects(tree, root);
    return root ? root->Rect() : LayoutRect{};
}

LayoutSize LayoutCore::MeasureNode(UiNode* node, const LayoutConstraints& constraints) {
    if (!node) {
        return LayoutSize{};
    }
    return engine_->Measure(node, constraints);
}

LayoutRect LayoutCore::ArrangeNode(UiComponentTree* tree, UiNode* node, const LayoutRect& finalRect) {
    if (!node) {
        return LayoutRect{};
    }

    avalang::ui::ILayoutNode* arranged = engine_->Arrange(node, finalRect);
    if (tree) {
        CacheRects(tree, arranged);
    }
    return arranged ? arranged->Rect() : LayoutRect{};
}

void LayoutCore::SetTextEvaluator(std::function<std::string(const std::string&)> eval) {
    engine_->SetTextEvaluator(std::move(eval));
}

bool LayoutCore::HasRect(const NodeId& id) const {
    return rects_.find(id) != rects_.end();
}

LayoutRect LayoutCore::RectOf(const NodeId& id) const {
    auto it = rects_.find(id);
    return it != rects_.end() ? it->second : LayoutRect{};
}

void LayoutCore::AdoptRects(const std::unordered_map<NodeId, LayoutRect>& rects) {
    rects_ = rects;
}

void LayoutCore::Clear() {
    rects_.clear();
}

void LayoutCore::CacheRects(UiComponentTree* tree, avalang::ui::ILayoutNode* layoutNode) {
    if (!tree || !layoutNode) {
        return;
    }

    UiNode* component = tree->FindById(layoutNode->Id());
    if (component) {
        rects_[component->NodeId()] = layoutNode->Rect();
    }

    for (avalang::ui::ILayoutNode* child : layoutNode->Children()) {
        CacheRects(tree, child);
    }
}

}
