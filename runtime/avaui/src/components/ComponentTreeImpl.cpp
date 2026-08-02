#include "components/ComponentTreeImpl.h"

namespace avalang {
namespace ui {
namespace components {

IComponent* ComponentTreeImpl::CreateComponent(const std::string& typeName) {
    ComponentId id = nextId_++;
    auto component = std::make_unique<Component>(id, typeName);
    IComponent* ptr = component.get();
    components_.emplace(id, std::move(component));
    return ptr;
}

void ComponentTreeImpl::DestroyComponent(ComponentId id) {
    auto it = components_.find(id);
    if (it == components_.end()) {
        return;
    }

    Component* comp = it->second.get();

    // Detach from parent's slot list.
    if (IComponent* parent = comp->Parent()) {
        parent->RemoveChild(comp);
    }

    // Orphan children instead of recursively destroying them (see
    // ComponentTree::DestroyComponent doc comment).
    for (IComponent* child : comp->Children()) {
        static_cast<Component*>(child)->SetParent(nullptr);
    }

    if (root_ == comp) {
        root_ = nullptr;
    }

    components_.erase(it);
}

IComponent* ComponentTreeImpl::FindById(ComponentId id) const {
    auto it = components_.find(id);
    return it == components_.end() ? nullptr : it->second.get();
}

IComponent* ComponentTreeImpl::Root() const {
    return root_;
}

void ComponentTreeImpl::SetRoot(IComponent* root) {
    root_ = root;
}

} // namespace components
} // namespace ui
} // namespace avalang
