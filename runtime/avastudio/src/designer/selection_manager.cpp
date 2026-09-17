#include "designer/selection_manager.h"

#include <algorithm>

namespace studio::designer {

void SelectionManager::Select(const NodeId& id, bool additive) {
    if (id.empty()) {
        return;
    }
    if (!additive) {
        selected_.clear();
        selected_.push_back(id);
        return;
    }
    Deselect(id);
    selected_.push_back(id);
}

void SelectionManager::SelectMany(const std::vector<NodeId>& ids) {
    selected_.clear();
    for (const NodeId& id : ids) {
        if (!id.empty() && !IsSelected(id)) {
            selected_.push_back(id);
        }
    }
}

void SelectionManager::Toggle(const NodeId& id) {
    if (id.empty()) {
        return;
    }
    if (IsSelected(id)) {
        Deselect(id);
    } else {
        selected_.push_back(id);
    }
}

void SelectionManager::Deselect(const NodeId& id) {
    selected_.erase(std::remove(selected_.begin(), selected_.end(), id), selected_.end());
}

void SelectionManager::Clear() {
    selected_.clear();
}

void SelectionManager::SetHovered(const NodeId& id) {
    hovered_ = id;
}

void SelectionManager::ClearHovered() {
    hovered_.clear();
}

void SelectionManager::SetFocused(const NodeId& id) {
    focused_ = id;
}

void SelectionManager::ClearFocused() {
    focused_.clear();
}

bool SelectionManager::IsSelected(const NodeId& id) const {
    return std::find(selected_.begin(), selected_.end(), id) != selected_.end();
}

bool SelectionManager::IsEmpty() const {
    return selected_.empty();
}

const std::vector<NodeId>& SelectionManager::Selected() const {
    return selected_;
}

NodeId SelectionManager::Primary() const {
    return selected_.empty() ? NodeId() : selected_.back();
}

const NodeId& SelectionManager::Hovered() const {
    return hovered_;
}

const NodeId& SelectionManager::Focused() const {
    return focused_;
}

}
