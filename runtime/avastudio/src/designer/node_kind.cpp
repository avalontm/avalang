#include "designer/node_kind.h"

#include <algorithm>
#include <cctype>

#include "design/component_catalog.h"

namespace studio::designer {

std::string LowerAscii(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

bool IsContainerType(const std::string& typeName) {
    const design::ComponentTypeInfo* info = design::FindComponentType(LowerAscii(typeName));
    return info != nullptr && info->is_container;
}

bool IsDialogNode(const UiNode* node) { return node != nullptr && LowerAscii(node->TypeName()) == "dialog"; }

}
