#include "state/StateBindingImpl.h"

#include <utility>

#include "components/IComponent.h"
#include "state/IState.h"

namespace avalang {
namespace ui {
namespace state {

StateBindingImpl::StateBindingImpl(IState* state, IComponent* component, std::string propertyName)
    : state_(state), component_(component), propertyName_(std::move(propertyName)) {
    if (!state_ || !component_) {
        // Create() documents this as a no-op binding -- nothing to
        // push, nothing to subscribe to, nothing to unsubscribe later.
        return;
    }

    // Sync immediately so the component doesn't wait for the state's
    // *next* change to reflect its *current* value.
    component_->SetProperty(propertyName_, state_->Value());

    subscriptionId_ = state_->Subscribe([this](const PropertyValue& newValue) {
        component_->SetProperty(propertyName_, newValue);
    });
    subscribed_ = true;
}

StateBindingImpl::~StateBindingImpl() {
    if (subscribed_) {
        state_->Unsubscribe(subscriptionId_);
    }
}

} // namespace state
} // namespace ui
} // namespace avalang
