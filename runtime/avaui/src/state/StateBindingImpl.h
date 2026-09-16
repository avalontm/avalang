#pragma once

#include <cstddef>
#include <string>

#include "state/StateBinding.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace state {

class StateBindingImpl final : public StateBinding, private common::NonCopyable {
public:
    StateBindingImpl(IState* state, IComponent* component, std::string propertyName);
    ~StateBindingImpl() override;

private:
    IState* state_;
    IComponent* component_;
    std::string propertyName_;
    std::size_t subscriptionId_ = 0;
    bool subscribed_ = false;
};

}
}
}