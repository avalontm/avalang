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
        return;
    }

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

}
}
}