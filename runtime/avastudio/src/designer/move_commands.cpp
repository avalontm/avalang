#include "designer/move_commands.h"

#include <utility>

namespace studio::designer {

namespace {

NodeId NextSiblingIdOf(UiNode* parent, UiNode* node) {
    if (!parent || !node) {
        return NodeId();
    }
    std::vector<UiNode*> siblings = parent->Children();
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i] == node) {
            return i + 1 < siblings.size() ? IdOf(siblings[i + 1]) : NodeId();
        }
    }
    return NodeId();
}

}

MoveComponentCommand::MoveComponentCommand(UiComponentTree* tree, NodeId movedId, NodeId targetId,
                                            studio::design::DropZone zone)
    : tree_(tree), movedId_(std::move(movedId)), targetId_(std::move(targetId)), zone_(zone) {}

void MoveComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    UiNode* root = tree_->Root();
    UiNode* moved = studio::design::FindNodeById(root, movedId_);
    if (!moved) {
        return;
    }

    if (!captured_) {
        UiNode* oldParent = studio::design::FindParentOf(root, moved);
        oldParentId_ = IdOf(oldParent);
        oldNextSiblingId_ = NextSiblingIdOf(oldParent, moved);
        captured_ = true;
    }

    studio::design::MoveNode(root, movedId_, targetId_, zone_);
}

void MoveComponentCommand::Redo() {
    if (!tree_) {
        return;
    }
    studio::design::MoveNode(tree_->Root(), movedId_, targetId_, zone_);
}

void MoveComponentCommand::Undo() {
    if (!tree_ || !captured_) {
        return;
    }
    UiNode* root = tree_->Root();

    if (!oldNextSiblingId_.empty()) {
        studio::design::MoveNode(root, movedId_, oldNextSiblingId_, studio::design::DropZone::kBefore);
    } else if (!oldParentId_.empty()) {
        studio::design::MoveNode(root, movedId_, oldParentId_, studio::design::DropZone::kInto);
    }
}

std::string MoveComponentCommand::Description() const {
    return "Move " + movedId_;
}

ReparentComponentCommand::ReparentComponentCommand(UiComponentTree* tree, NodeId nodeId, NodeId newParentId)
    : MoveComponentCommand(tree, std::move(nodeId), std::move(newParentId), studio::design::DropZone::kInto) {}

std::string ReparentComponentCommand::Description() const {
    return "Reparent " + movedId_ + " -> " + targetId_;
}

}
