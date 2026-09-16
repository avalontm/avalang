#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include "layout/LayoutEngine.h"
#include "common/NonCopyable.h"
#include "layout/LayoutNode.h"

namespace avalang {
namespace ui {
namespace layout {

struct IntrinsicSize {
    double width = 0.0;
    double height = 0.0;
};

class LayoutEngineImpl final : public LayoutEngine, private common::NonCopyable {
public:
    ILayoutNode* Compute(IComponent* componentRoot, const LayoutRect& available) override;
    ILayoutNode* ComputeFromPhysicalPixels(IComponent* componentRoot, const LayoutRect& physicalAvailable) override;
    LayoutSize Measure(IComponent* componentRoot, const LayoutConstraints& constraints) override;
    ILayoutNode* Arrange(IComponent* componentRoot, const LayoutRect& finalRect) override;
    void SetTextEvaluator(std::function<std::string(const std::string&)> eval) override {
        textEvaluator_ = std::move(eval);
    }
    ILayoutNode* FindNode(ComponentId id) const override;
    ILayoutNode* Root() const override;

private:
    std::string EvalText(const std::string& raw) const {
        return textEvaluator_ ? textEvaluator_(raw) : raw;
    }

    std::function<std::string(const std::string&)> textEvaluator_;

    LayoutNode* BuildTree(IComponent* component, LayoutNode* parent);

    IntrinsicSize ComputeIntrinsicSize(IComponent* component);

    void LayoutNodeRecursive(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                              LayoutAlignment hAlign, LayoutAlignment vAlign);

    LayoutRect PlaceComponent(IComponent* component, LayoutNode* node, const LayoutRect& slot,
                               LayoutAlignment hAlign, LayoutAlignment vAlign);

    void ArrangeRowOrColumn(IComponent* component, LayoutNode* node, const LayoutRect& contentBox, bool isRow, bool allowOverflow = false);

    void ArrangeStack(IComponent* component, LayoutNode* node, const LayoutRect& contentBox);

    void ArrangeGrid(IComponent* component, LayoutNode* node, const LayoutRect& contentBox);

    std::unordered_map<ComponentId, std::unique_ptr<LayoutNode>> nodes_;
    LayoutNode* root_ = nullptr;

    LayoutRect rootViewport_;

    std::unordered_map<ComponentId, IntrinsicSize> intrinsic_;

    std::unordered_map<ComponentId, std::pair<unsigned long long, IntrinsicSize>> intrinsicCache_;
};

}
}
}