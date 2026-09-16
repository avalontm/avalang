#pragma once

#include <memory>
#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/IComponent.h"
#include "state/IState.h"

namespace avalang {
namespace ui {

class AVA_UI_API StateBinding {
public:
    static std::unique_ptr<StateBinding> Create(IState* state, IComponent* component, std::string propertyName);

    virtual ~StateBinding() = default;
};

}
}