#include "designer/property_commands.h"

#include <utility>

#include "design/design_document.h"

namespace studio::designer {

SetPropertyCommand::SetPropertyCommand(UiComponentTree* tree, NodeId nodeId, std::string property,
                                        PropertyValue newValue)
    : tree_(tree), nodeId_(std::move(nodeId)), property_(std::move(property)), newValue_(std::move(newValue)) {}

void SetPropertyCommand::Execute() {
    if (!tree_) {
        return;
    }
    UiNode* node = studio::design::FindNodeById(tree_->Root(), nodeId_);
    if (!node) {
        return;
    }

    const PropertyValue* current = node->GetProperty(property_);
    hadOldValue_ = current != nullptr;
    if (hadOldValue_) {
        oldValue_ = *current;
    }

    node->SetProperty(property_, newValue_);
}

void SetPropertyCommand::Undo() {
    Apply(hadOldValue_, oldValue_);
}

void SetPropertyCommand::Redo() {
    Apply(true, newValue_);
}

void SetPropertyCommand::Apply(bool hasValue, const PropertyValue& value) {
    if (!tree_) {
        return;
    }
    UiNode* node = studio::design::FindNodeById(tree_->Root(), nodeId_);
    if (!node) {
        return;
    }

    if (hasValue) {
        node->SetProperty(property_, value);
    } else {
        node->RemoveProperty(property_);
    }
}

std::string SetPropertyCommand::Description() const {
    return "Set " + property_;
}

RemovePropertyCommand::RemovePropertyCommand(UiComponentTree* tree, NodeId nodeId, std::string property)
    : tree_(tree), nodeId_(std::move(nodeId)), property_(std::move(property)) {}

void RemovePropertyCommand::Execute() {
    if (!tree_) {
        return;
    }
    UiNode* node = studio::design::FindNodeById(tree_->Root(), nodeId_);
    if (!node) {
        return;
    }

    const PropertyValue* current = node->GetProperty(property_);
    hadOldValue_ = current != nullptr;
    if (hadOldValue_) {
        oldValue_ = *current;
    }

    node->RemoveProperty(property_);
}

void RemovePropertyCommand::Redo() {
    Remove();
}

void RemovePropertyCommand::Remove() {
    if (!tree_) {
        return;
    }
    if (UiNode* node = studio::design::FindNodeById(tree_->Root(), nodeId_)) {
        node->RemoveProperty(property_);
    }
}

void RemovePropertyCommand::Undo() {
    if (!tree_ || !hadOldValue_) {
        return;
    }
    if (UiNode* node = studio::design::FindNodeById(tree_->Root(), nodeId_)) {
        node->SetProperty(property_, oldValue_);
    }
}

std::string RemovePropertyCommand::Description() const {
    return "Remove " + property_;
}

SetStyleCommand::SetStyleCommand(UiComponentTree* tree, NodeId nodeId, std::string styleName)    : SetPropertyCommand(tree, std::move(nodeId), "style", PropertyValue(std::move(styleName))) {}

std::string SetStyleCommand::Description() const {
    return "Set style " + newValue_.AsString();
}

SetEventCommand::SetEventCommand(UiComponentTree* tree, NodeId nodeId, std::string eventName,
                                  std::string handlerName)
    : SetPropertyCommand(tree, std::move(nodeId), std::move(eventName),
                          PropertyValue(std::move(handlerName))) {}

std::string SetEventCommand::Description() const {
    return "Set " + property_ + " handler";
}

}
