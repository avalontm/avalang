#pragma once

#include "Export.h"
#include "Fwd.h"

#include <vector>

namespace avalang {
namespace ui {
namespace events {

struct InteractiveNode {
    ComponentId id = 0;
    ComponentId owner = 0;
    bool enabled = true;
    bool focusable = true;
};

AVA_UI_API bool OwnsInteractionSubtree(const IComponent* component);

AVA_UI_API InteractiveNode ResolveInteractiveNode(const IComponent* component);

AVA_UI_API bool IsOverlayOpen(const IComponent* component);

AVA_UI_API bool IsBlockingOverlay(const IComponent* component);

AVA_UI_API bool IsDismissibleOverlay(const IComponent* component);

AVA_UI_API IComponent* FindTopmostBlockingOverlay(IComponent* root);

AVA_UI_API bool IsDescendantOf(const IComponent* node, const IComponent* ancestor);

AVA_UI_API void CollectFocusableDescendants(IComponent* root, std::vector<IComponent*>& out);

}
}
}
