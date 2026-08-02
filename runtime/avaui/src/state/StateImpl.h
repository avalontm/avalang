#ifndef AVA_UI_STATE_STATEIMPL_H
#define AVA_UI_STATE_STATEIMPL_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "state/IState.h"
#include "common/NonCopyable.h"

namespace avalang {
namespace ui {
namespace state {

// Concrete IState. Internal -- consumers only ever see IState*,
// obtained from IState::Create(). Plain value + subscriber map, no
// component/layout awareness (see IState.h).
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

} // namespace state
} // namespace ui
} // namespace avalang

#endif // AVA_UI_STATE_STATEIMPL_H
