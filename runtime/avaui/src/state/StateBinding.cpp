#include "state/StateBinding.h"

#include "state/StateBindingImpl.h"

namespace avalang {
namespace ui {

std::unique_ptr<StateBinding> StateBinding::Create(IState* state, IComponent* component, std::string propertyName) {
    return std::make_unique<state::StateBindingImpl>(state, component, std::move(propertyName));
}

} // namespace ui
} // namespace avalang
