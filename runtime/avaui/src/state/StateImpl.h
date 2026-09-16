#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "state/IState.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace state {

class StateImpl final : public IState, private common::NonCopyable {
public:
    explicit StateImpl(PropertyValue initial);

    const PropertyValue& Value() const override;
    void Set(PropertyValue value) override;

    std::size_t Subscribe(ChangeHandler handler) override;
    void Unsubscribe(std::size_t subscriptionId) override;

private:
    PropertyValue value_;
    std::unordered_map<std::size_t, ChangeHandler> handlers_;
    std::size_t nextSubscriptionId_ = 1;
};

}
}
}