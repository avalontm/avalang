#pragma once

#include <string>
#include <vector>

#include "designer/types.h"

namespace studio::designer {

struct DocumentTreeNode {
    NodeId id;
    ComponentTypeId type;
    std::string displayName;
    int depth = 0;
    bool hasChildren = false;
};

std::string DisplayNameFor(UiNode* node);

std::vector<DocumentTreeNode> BuildDocumentTree(UiComponentTree* tree);

}
