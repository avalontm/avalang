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
    static std::unique_ptr<AccessibilityTree> Create(ComponentTree* componentTree);

    AccessibilityNode* Root() const;
    const std::vector<std::unique_ptr<AccessibilityNode>>& Nodes() const;

    AccessibilityNode* FindById(ComponentId id) const;

private:
    AccessibilityTree() = default;
    AccessibilityTree(const AccessibilityTree&) = delete;
    AccessibilityTree& operator=(const AccessibilityTree&) = delete;

    void Build(ComponentTree* componentTree);
    AccessibilityNode* BuildNode(IComponent* component, AccessibilityNode* parent);

    std::vector<std::unique_ptr<AccessibilityNode>> nodes_;
    AccessibilityNode* root_ = nullptr;
};

}
}
}