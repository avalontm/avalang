#include "state/IState.h"

#include "state/StateImpl.h"

namespace avalang {
namespace ui {

std::unique_ptr<IState> IState::Create(PropertyValue initial) {
    return std::make_unique<state::StateImpl>(std::move(initial));
}

} // namespace ui
} // namespace avalang
