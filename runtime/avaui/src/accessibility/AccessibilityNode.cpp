#include "accessibility/AccessibilityNode.h"

namespace avalang {
namespace ui {
namespace accessibility {

AccessibilityNode::AccessibilityNode(ComponentId id, AccessibilityRole role)
    : id_(id), role_(role) {}

ComponentId AccessibilityNode::Id() const {
    return id_;
}

AccessibilityRole AccessibilityNode::Role() const {
    return role_;
}

void AccessibilityNode::SetRole(AccessibilityRole role) {
    role_ = role;
}

const std::string& AccessibilityNode::Label() const {
    return label_;
}

void AccessibilityNode::SetLabel(const std::string& label) {
    label_ = label;
}

const std::string& AccessibilityNode::Value() const {
    return value_;
}

void AccessibilityNode::SetValue(const std::string& value) {
    value_ = value;
}

const std::string& AccessibilityNode::Hint() const {
    return hint_;
}

void AccessibilityNode::SetHint(const std::string& hint) {
    hint_ = hint;
}

uint32_t AccessibilityNode::States() const {
    return states_;
}

void AccessibilityNode::AddState(AccessibilityState state) {
    states_ |= static_cast<uint32_t>(state);
}

void AccessibilityNode::RemoveState(AccessibilityState state) {
    states_ &= ~static_cast<uint32_t>(state);
}

bool AccessibilityNode::HasState(AccessibilityState state) const {
    return (states_ & static_cast<uint32_t>(state)) != 0;
}

const std::vector<AccessibilityAction>& AccessibilityNode::Actions() const {
    return actions_;
}

void AccessibilityNode::AddAction(AccessibilityAction action) {
    for (AccessibilityAction existing : actions_) {
        if (existing == action) return;
    }
    actions_.push_back(action);
}

const std::vector<AccessibilityNode*>& AccessibilityNode::Children() const {
    return children_;
}

void AccessibilityNode::AddChild(AccessibilityNode* child) {
    children_.push_back(child);
}

AccessibilityNode* AccessibilityNode::Parent() const {
    return parent_;
}

void AccessibilityNode::SetParent(AccessibilityNode* parent) {
    parent_ = parent;
}

}
}
}