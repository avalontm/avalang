#ifndef AVA_UI_COMPONENTS_COMPONENTTREE_H
#define AVA_UI_COMPONENTS_COMPONENTTREE_H

#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/IComponent.h"

namespace avalang {
namespace ui {

// Owns every IComponent instance created through it. Phase 2: a plain
// in-memory tree -- parsing (.avaui -> ComponentTree) belongs to
// parser/ (a later phase); layout and rendering are Phases 3 and 6+.
class AVA_UI_API ComponentTree {
public:
    static std::unique_ptr<ComponentTree> Create();

    virtual ~ComponentTree() = default;

    // Creates a new, parentless component of the given type and returns
    // a non-owning pointer (lifetime owned by this tree). typeName is a
    // free-form string (e.g. "Page", "Button", or a future custom
    // component name) -- Phase 2 attaches no behavior to it, only an
    // identity.
    virtual IComponent* CreateComponent(const std::string& typeName) = 0;

    // Destroys a component: detaches it from its parent's slot, and
    // orphans its children (their Parent() becomes nullptr) without
    // destroying them -- the caller decides what happens to orphaned
    // children next (reparent or destroy explicitly). No-op if `id`
    // does not exist in this tree.
    virtual void DestroyComponent(ComponentId id) = 0;

    virtual IComponent* FindById(ComponentId id) const = 0;

    virtual IComponent* Root() const = 0;
    virtual void SetRoot(IComponent* root) = 0;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_COMPONENTTREE_H
