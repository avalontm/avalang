#ifndef AVA_UI_STATE_STATEBINDINGIMPL_H
#define AVA_UI_STATE_STATEBINDINGIMPL_H

#include <cstddef>
#include <string>

#include "state/StateBinding.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace state {

// Concrete StateBinding. Internal -- consumers only ever see
// StateBinding*, obtained from StateBinding::Create(). See
// StateBinding.h for the push-only, RAII-unsubscribe contract this
// implements.
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

} // namespace state
} // namespace ui
} // namespace avalang

#endif // AVA_UI_STATE_STATEBINDINGIMPL_H
