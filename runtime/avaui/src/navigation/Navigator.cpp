#include "navigation/Navigator.h"

#include <utility>

namespace avalang {
namespace ui {
namespace navigation {

std::unique_ptr<INavigator> INavigator::Create() {
    return std::make_unique<Navigator>();
}

void Navigator::SetHandler(NavigationHandler handler) {
    handler_ = std::move(handler);
}

void Navigator::Push(Route route) {
    stack_.Push(std::move(route));
    Notify(NavigationType::Push);
}

void Navigator::Replace(Route route) {
    stack_.Replace(std::move(route));
    Notify(NavigationType::Replace);
}

bool Navigator::Back() {
    if (!stack_.CanPop()) return false;
    stack_.Pop();
    Notify(NavigationType::Pop);
    return true;
}

bool Navigator::CanBack() const {
    return stack_.CanPop();
}

const Route* Navigator::Current() const {
    return stack_.Top();
}

size_t Navigator::Depth() const {
    return stack_.Depth();
}

void Navigator::Clear() {
    stack_.Clear();
}

void Navigator::Notify(NavigationType type) {
    if (!handler_) return;
    const Route* current = Current();
    if (!current) return;
    handler_(NavigationContext{*current, type, stack_.Depth()});
}

}
}
}