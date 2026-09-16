#include "state/StateImpl.h"

#include <utility>

#include "state/PropertyValueEquals.h"

namespace avalang {
namespace ui {
namespace state {

StateImpl::StateImpl(PropertyValue initial) : value_(std::move(initial)) {}

const PropertyValue& StateImpl::Value() const {
    return value_;
}

void StateImpl::Set(PropertyValue value) {
    if (PropertyValueEquals(value_, value)) {
        return;
    }
    value_ = std::move(value);

    std::vector<ChangeHandler> snapshot;
    snapshot.reserve(handlers_.size());
    for (const auto& entry : handlers_) {
        snapshot.push_back(entry.second);
    }

    for (const ChangeHandler& handler : snapshot) {
        if (handler) {
            handler(value_);
        }
    }
}

std::size_t StateImpl::Subscribe(ChangeHandler handler) {
    std::size_t id = nextSubscriptionId_++;
    handlers_.emplace(id, std::move(handler));
    return id;
}

void StateImpl::Unsubscribe(std::size_t subscriptionId) {
    handlers_.erase(subscriptionId);
}

}
}
}