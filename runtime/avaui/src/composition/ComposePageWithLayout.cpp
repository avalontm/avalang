#include "composition/ComposePageWithLayout.h"

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "layout/LayoutEngine.h"

namespace avalang {
namespace ui {

namespace {

IComponent* FindComponentByType(IComponent* root, const std::string& typeName) {
    if (!root) return nullptr;
    if (root->TypeName() == typeName) return root;
    for (IComponent* child : root->Children()) {
        if (IComponent* found = FindComponentByType(child, typeName)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

ComposedSlotInfo LocateLayoutSlot(ComponentTree* layoutTree,
                                  int viewportWidth,
                                  int viewportHeight) {
    ComposedSlotInfo result;
    result.slotRect = {0.0, 0.0,
                       static_cast<double>(viewportWidth),
                       static_cast<double>(viewportHeight)};
    result.hasSlot = false;

    if (!layoutTree || !layoutTree->Root()) return result;

    IComponent* slotComponent = FindComponentByType(layoutTree->Root(), "Slot");
    if (!slotComponent) return result;

    auto layoutEngine = LayoutEngine::Create();
    LayoutRect viewport{0.0, 0.0,
                        static_cast<double>(viewportWidth),
                        static_cast<double>(viewportHeight)};
    layoutEngine->Compute(layoutTree->Root(), viewport);

    if (ILayoutNode* slotNode = layoutEngine->FindNode(slotComponent->Id())) {
        result.slotRect = slotNode->Rect();
        result.hasSlot = true;
    }

    return result;
}

} // namespace ui
} // namespace avalang
