#include "designer/document_tree.h"

#include <vector>


namespace studio::designer {

namespace {

void CollectNodes(UiNode* node, int depth, std::vector<DocumentTreeNode>& out) {
    if (!node) {
        return;
    }
    std::vector<UiNode*> children = node->Children();

    DocumentTreeNode entry;
    entry.id = IdOf(node);
    entry.type = TypeOf(node);
    entry.displayName = DisplayNameFor(node);
    entry.depth = depth;
    entry.hasChildren = !children.empty();
    out.push_back(entry);

    for (UiNode* child : children) {
        CollectNodes(child, depth + 1, out);
    }
}

}

std::string DisplayNameFor(UiNode* node) {
    if (!node) {
        return std::string();
    }
    const PropertyValue* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
        return idProp->AsString();
    }
    return TypeOf(node);
}

std::vector<DocumentTreeNode> BuildDocumentTree(UiComponentTree* tree) {
    std::vector<DocumentTreeNode> nodes;
    if (tree) {
        CollectNodes(tree->Root(), 0, nodes);
    }
    return nodes;
}

}
