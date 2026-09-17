#pragma once

#include <unordered_set>
#include <vector>

#include "designer/document_tree.h"
#include "designer/types.h"

namespace studio::designer {

class TreeState {
public:
    bool IsExpanded(const NodeId& id) const;
    void SetExpanded(const NodeId& id, bool expanded);
    void Toggle(const NodeId& id);
    void ExpandAncestorsOf(const NodeId& id, UiComponentTree* tree);

    std::vector<DocumentTreeNode> VisibleNodes(UiComponentTree* tree) const;

private:
    std::unordered_set<NodeId> collapsed_;
};

}
