#include "controls/Button.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"
#include <unordered_map>
#include <mutex>

namespace avalang {
namespace ui {
namespace controls {

namespace {

std::unordered_map<ComponentId, ButtonClickCallback> g_buttonCallbacks;
std::mutex g_callbackMutex;

}

IComponent* CreateButton(ComponentTree* tree, const std::string& text) {
    if (!tree) {
        return nullptr;
    }

    IComponent* button = tree->CreateComponent("Button");
    if (!button) {
        return nullptr;
    }

    button->SetProperty("text", PropertyValue(text));
    button->SetProperty("isEnabled", PropertyValue(true));
    button->SetProperty("style", PropertyValue(std::string("primary")));

    return button;
}

void BindButtonClick(ComponentId buttonId, ButtonClickCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_buttonCallbacks[buttonId] = callback;
}

void UnbindButtonClick(ComponentId buttonId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_buttonCallbacks.erase(buttonId);
}

namespace internal {

ButtonClickCallback* GetButtonClickCallback(ComponentId buttonId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    auto it = g_buttonCallbacks.find(buttonId);
    return (it != g_buttonCallbacks.end()) ? &it->second : nullptr;
}

}

namespace {
struct ButtonTypeRegistration {
    ButtonTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Button", "Button", false,
            WithCommonProperties(CommonInteractiveProperties(), {
                PropertyDescriptor{
                    .name = "text",
                    .defaultValue = PropertyValue("Button"),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Typography,
                    .description = "Texto visible dentro del botón",
                },
                PropertyDescriptor{
                    .name = "style",
                    .defaultValue = PropertyValue("primary"),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Appearance,
                    .description = "Nombre de estilo o clase resuelto contra el theme/stylesheet "
                                   "del proyecto (ej. \"primary\", \"secondary\", o una clase "
                                   "personalizada definida en el stylesheet)",
                },
            }),
        });
    }
};
static ButtonTypeRegistration _button_type_registration;
}

}
}
}