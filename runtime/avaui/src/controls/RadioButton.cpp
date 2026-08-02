#include "controls/RadioButton.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>

namespace avalang::ui::controls {

namespace {

// group name -> member components (process-wide; see header comment).
std::unordered_map<std::string, std::vector<IComponent*>> g_groups;
std::unordered_map<ComponentId, RadioButtonChangeCallback> g_callbacks;
std::mutex g_mutex;

void NotifyChange(IComponent* comp, bool isSelected) {
    RadioButtonChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_callbacks.find(comp->Id());
        if (it != g_callbacks.end()) {
            callback = it->second;
        }
    }
    if (callback) {
        callback(comp->Id(), isSelected);
    }
}

} // anonymous namespace

IComponent* CreateRadioButton(ComponentTree* tree, const std::string& label,
                               const std::string& group, bool isSelected) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("RadioButton");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("label", PropertyValue(label));
    comp->SetProperty("group", PropertyValue(group));
    comp->SetProperty("isSelected", PropertyValue(false));
    comp->SetProperty("isEnabled", PropertyValue(true));

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_groups[group].push_back(comp);
    }

    if (isSelected) {
        SelectRadioButton(comp);
    }

    // borderColor / borderWidth filled by RenderTheme::Apply().
    return comp;
}

void SelectRadioButton(IComponent* radioButtonComponent) {
    if (!radioButtonComponent) {
        return;
    }

    const auto* groupProp = radioButtonComponent->GetProperty("group");
    std::string group = (groupProp && groupProp->Type() == PropertyType::String)
                             ? groupProp->AsString()
                             : std::string();

    std::vector<IComponent*> members;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_groups.find(group);
        if (it != g_groups.end()) {
            members = it->second;
        }
    }

    for (IComponent* member : members) {
        if (!member) continue;
        bool shouldBeSelected = (member == radioButtonComponent);
        const auto* current = member->GetProperty("isSelected");
        bool wasSelected = current && current->Type() == PropertyType::Bool && current->AsBool();
        if (wasSelected == shouldBeSelected) {
            continue; // no change, no callback
        }
        member->SetProperty("isSelected", PropertyValue(shouldBeSelected));
        NotifyChange(member, shouldBeSelected);
    }

    // In case radioButtonComponent wasn't found in its own group (e.g.
    // group changed after creation), make sure it's still selected.
    const auto* selfProp = radioButtonComponent->GetProperty("isSelected");
    if (!selfProp || selfProp->Type() != PropertyType::Bool || !selfProp->AsBool()) {
        radioButtonComponent->SetProperty("isSelected", PropertyValue(true));
        NotifyChange(radioButtonComponent, true);
    }
}

bool GetRadioButtonSelected(IComponent* radioButtonComponent) {
    if (!radioButtonComponent) {
        return false;
    }
    const auto* prop = radioButtonComponent->GetProperty("isSelected");
    if (!prop || prop->Type() != PropertyType::Bool) {
        return false;
    }
    return prop->AsBool();
}

void BindRadioButtonChange(ComponentId radioButtonId, RadioButtonChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbacks[radioButtonId] = std::move(callback);
}

void UnbindRadioButtonChange(ComponentId radioButtonId) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbacks.erase(radioButtonId);
}

} // namespace avalang::ui::controls
