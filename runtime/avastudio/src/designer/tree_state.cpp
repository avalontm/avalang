#include "designer/tree_state.h"

#include "design/design_document.h"

namespace studio::designer {

bool TreeState::IsExpanded(const NodeId& id) const {
    return collapsed_.find(id) == collapsed_.end();
}

void TreeState::SetExpanded(const NodeId& id, bool expanded) {
    if (expanded) {
        collapsed_.erase(id);
    } else {
        collapsed_.insert(id);
    }
}

void TreeState::Toggle(const NodeId& id) {
    SetExpanded(id, !IsExpanded(id));
}

void TreeState::ExpandAncestorsOf(const NodeId& id, UiComponentTree* tree) {
    if (!tree) {
        return;
    }
    UiNode* node = studio::design::FindNodeById(tree->Root(), id);
    if (!node) {
        return;
    }
    for (UiNode* parent = node->Parent(); parent; parent = parent->Parent()) {
        SetExpanded(IdOf(parent), true);
    }
}

std::vector<DocumentTreeNode> TreeState::VisibleNodes(UiComponentTree* tree) const {
    std::vector<DocumentTreeNode> all = BuildDocumentTree(tree);
    std::vector<DocumentTreeNode> visible;

    int hiddenBelowDepth = -1;
    for (const DocumentTreeNode& node : all) {
        if (hiddenBelowDepth >= 0) {
            if (node.depth > hiddenBelowDepth) {
                continue;
            }
            hiddenBelowDepth = -1;
        }

        visible.push_back(node);

        if (node.hasChildren && !IsExpanded(node.id)) {
            hiddenBelowDepth = node.depth;
        }
    }

    return visible;
}

}
