#include "designer/component_extraction.h"

#include "design/design_document.h"
#include "designer/component_registry.h"

namespace studio::designer {

ExtractionCandidate ValidateExtractionCandidate(UiComponentTree* tree, NodeId nodeId) {
    if (!tree || !tree->Root()) {
        return {false, "No document loaded"};
    }

    UiNode* node = studio::design::FindNodeById(tree->Root(), nodeId);
    if (!node) {
        return {false, "Node not found"};
    }

    if (node == tree->Root()) {
        return {false, "The document root cannot be extracted"};
    }

    if (const PropertyValue* call = node->GetProperty("__unresolvedImportCall")) {
        if (call->Type() == PropertyType::Bool && call->AsBool()) {
            return {false, "A component reference cannot be extracted"};
        }
    }

    const ComponentMetadata* metadata = ComponentRegistry::Instance().Find(TypeOf(node));
    if (metadata && !metadata->designerCapabilities.canDuplicate) {
        return {false, "This component type cannot be duplicated/extracted"};
    }

    return {true, std::string()};
}

}
