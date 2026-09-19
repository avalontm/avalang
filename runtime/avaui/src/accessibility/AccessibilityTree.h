#pragma once

#include <memory>
#include <vector>

#include "Export.h"
#include "accessibility/AccessibilityNode.h"
#include "components/ComponentTree.h"

namespace avalang {
namespace ui {
namespace accessibility {

class AVA_UI_API AccessibilityTree {
public:
    static std::unique_ptr<AccessibilityTree> Create(ComponentTree* componentTree,
                                                       LayoutEngine* layoutEngine = nullptr,
                                                       ComponentId focusedId = 0);

    AccessibilityNode* Root() const;
    const std::vector<std::unique_ptr<AccessibilityNode>>& Nodes() const;

    AccessibilityNode* FindById(ComponentId id) const;

private:
    AccessibilityTree() = default;
    AccessibilityTree(const AccessibilityTree&) = delete;
    AccessibilityTree& operator=(const AccessibilityTree&) = delete;

    void Build(ComponentTree* componentTree, LayoutEngine* layoutEngine, ComponentId focusedId);
    AccessibilityNode* BuildNode(IComponent* component, AccessibilityNode* parent,
                                  LayoutEngine* layoutEngine, ComponentId focusedId);

    std::vector<std::unique_ptr<AccessibilityNode>> nodes_;
    AccessibilityNode* root_ = nullptr;
};

}
}
}