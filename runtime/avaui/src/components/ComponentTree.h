#pragma once

#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/IComponent.h"

namespace avalang {
namespace ui {

class AVA_UI_API ComponentTree {
public:
    static std::unique_ptr<ComponentTree> Create();

    virtual ~ComponentTree() = default;

    virtual IComponent* CreateComponent(const std::string& typeName) = 0;

    virtual void DestroyComponent(ComponentId id) = 0;

    virtual IComponent* FindById(ComponentId id) const = 0;

    virtual IComponent* Root() const = 0;
    virtual void SetRoot(IComponent* root) = 0;
};

}
}