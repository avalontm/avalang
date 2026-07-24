#ifndef AVA_UI_TREE_H
#define AVA_UI_TREE_H

#include <memory>

#include "ui/component.h"

namespace ava {
namespace ui {

// Owns the root of a Component Tree produced by running/loading an
// AvaLang UI script. See docs referenced from PROGRESS.md:
// "AvaLang.UI (framework de componentes C# + HTML renderer) ... siguiendo
// el Component Tree definido en docs/architecture/01_COMPONENT_TREE_AND_DSL.md".
class ComponentTree {
public:
    ComponentTree() = default;

    void SetRoot(std::shared_ptr<Component> root) { root_ = std::move(root); }
    std::shared_ptr<Component> GetRoot() const { return root_; }

private:
    std::shared_ptr<Component> root_;
};

} // namespace ui
} // namespace ava

#endif // AVA_UI_TREE_H
