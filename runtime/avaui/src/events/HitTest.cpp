#include "events/HitTest.h"
#include "components/IComponent.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace avalang {
namespace ui {
namespace events {

namespace {

std::string Lowercase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool AsBool(const IComponent* component, const std::string& name, bool defaultValue) {
    const PropertyValue* value = component->GetProperty(name);
    if (!value || value->Type() != PropertyType::Bool) return defaultValue;
    return value->AsBool();
}

}

bool OwnsInteractionSubtree(const IComponent* component) {
    if (!component) return false;

    const std::string typeLower = Lowercase(component->TypeName());
    return typeLower == "button" ||
           typeLower == "link" ||
           typeLower == "textbox" ||
           typeLower == "checkbox" ||
           typeLower == "radiobutton" ||
           typeLower == "combobox";
}

InteractiveNode ResolveInteractiveNode(const IComponent* component) {
    InteractiveNode node;
    if (!component) return node;

    node.id = component->Id();
    node.owner = component->Id();
    node.enabled = AsBool(component, "isEnabled", true);
    node.focusable = node.enabled && OwnsInteractionSubtree(component);
    return node;
}

bool IsOverlayOpen(const IComponent* component) {
    if (!component) return false;
    if (!AsBool(component, "overlay", false)) return false;
    if (!component->HasProperty("isOpen")) return true;
    return AsBool(component, "isOpen", false);
}

bool IsBlockingOverlay(const IComponent* component) {
    return IsOverlayOpen(component) && AsBool(component, "backdrop", false);
}

bool IsDismissibleOverlay(const IComponent* component) {
    return AsBool(component, "dismissible", true);
}

namespace {

double OverlayPriorityOf(const IComponent* component) {
    const PropertyValue* value = component->GetProperty("zIndex");
    if (!value || value->Type() != PropertyType::Number) return 0.0;
    return value->AsNumber();
}

void CollectBlockingOverlays(IComponent* node, std::vector<IComponent*>& out) {
    if (!node) return;
    if (IsBlockingOverlay(node)) {
        out.push_back(node);
    }
    for (auto* child : node->Children()) {
        CollectBlockingOverlays(child, out);
    }
}

}

IComponent* FindTopmostBlockingOverlay(IComponent* root) {
    std::vector<IComponent*> overlays;
    CollectBlockingOverlays(root, overlays);
    if (overlays.empty()) return nullptr;

    IComponent* topmost = overlays.front();
    double topmostPriority = OverlayPriorityOf(topmost);
    for (auto* overlay : overlays) {
        double priority = OverlayPriorityOf(overlay);
        if (priority >= topmostPriority) {
            topmost = overlay;
            topmostPriority = priority;
        }
    }
    return topmost;
}

bool IsDescendantOf(const IComponent* node, const IComponent* ancestor) {
    if (!node || !ancestor) return false;
    const IComponent* current = node;
    while (current) {
        if (current == ancestor) return true;
        current = current->Parent();
    }
    return false;
}

void CollectFocusableDescendants(IComponent* root, std::vector<IComponent*>& out) {
    if (!root) return;

    if (OwnsInteractionSubtree(root) && AsBool(root, "isEnabled", true)) {
        out.push_back(root);
        return;
    }

    for (auto* child : root->Children()) {
        CollectFocusableDescendants(child, out);
    }
}

}
}
}
