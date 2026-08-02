#ifndef AVA_UI_COMPONENTS_COMPONENTTREEIMPL_H
#define AVA_UI_COMPONENTS_COMPONENTTREEIMPL_H

#include <memory>
#include <unordered_map>

#include "components/ComponentTree.h"
#include "components/Component.h"

namespace avalang {
namespace ui {
namespace components {

class ComponentTreeImpl final : public ComponentTree {
public:
    IComponent* CreateComponent(const std::string& typeName) override;
    void DestroyComponent(ComponentId id) override;
    IComponent* FindById(ComponentId id) const override;
    IComponent* Root() const override;
    void SetRoot(IComponent* root) override;

private:
    std::unordered_map<ComponentId, std::unique_ptr<Component>> components_;
    ComponentId nextId_ = 1;
    IComponent* root_ = nullptr;
};

} // namespace components
} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMPONENTS_COMPONENTTREEIMPL_H
