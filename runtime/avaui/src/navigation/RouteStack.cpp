#include "navigation/RouteStack.h"

namespace avalang {
namespace ui {
namespace navigation {

void RouteStack::Push(Route route) {
    entries_.push_back(std::move(route));
}

void RouteStack::Replace(Route route) {
    if (entries_.empty()) {
        entries_.push_back(std::move(route));
    } else {
        entries_.back() = std::move(route);
    }
}

bool RouteStack::Pop() {
    if (entries_.size() <= 1) return false;
    entries_.pop_back();
    return true;
}

bool RouteStack::CanPop() const {
    return entries_.size() > 1;
}

size_t RouteStack::Depth() const {
    return entries_.size();
}

const Route* RouteStack::Top() const {
    if (entries_.empty()) return nullptr;
    return &entries_.back();
}

void RouteStack::Clear() {
    entries_.clear();
}

}
}
}