#include "controls/TextBox.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"
#include <unordered_map>
#include <mutex>

namespace avalang {
namespace ui {
namespace controls {

namespace {

std::unordered_map<ComponentId, TextBoxChangeCallback> g_textBoxCallbacks;
std::mutex g_callbackMutex;

}

IComponent* CreateTextBox(ComponentTree* tree, const std::string& placeholder) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("TextBox");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("text", PropertyValue(std::string()));
    comp->SetProperty("placeholder", PropertyValue(placeholder));
    comp->SetProperty("isFocused", PropertyValue(false));
    comp->SetProperty("isEnabled", PropertyValue(true));
    comp->SetProperty("caretIndex", PropertyValue(0.0));
    comp->SetProperty("selectionAnchor", PropertyValue(0.0));
    comp->SetProperty("imeComposition", PropertyValue(std::string()));
    comp->SetProperty("imeCompositionCursor", PropertyValue(0.0));

    return comp;
}

void SetTextBoxValue(IComponent* textBoxComponent, const std::string& value) {
    if (!textBoxComponent) {
        return;
    }
    textBoxComponent->SetProperty("text", PropertyValue(value));

    TextBoxChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        auto it = g_textBoxCallbacks.find(textBoxComponent->Id());
        if (it != g_textBoxCallbacks.end()) {
            callback = it->second;
        }
    }
    if (callback) {
        callback(textBoxComponent->Id(), value);
    }
}

std::string GetTextBoxValue(IComponent* textBoxComponent) {
    if (!textBoxComponent) {
        return std::string();
    }
    const auto* prop = textBoxComponent->GetProperty("text");
    if (!prop || prop->Type() != PropertyType::String) {
        return std::string();
    }
    return prop->AsString();
}

void BindTextBoxChange(ComponentId textBoxId, TextBoxChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_textBoxCallbacks[textBoxId] = std::move(callback);
}

void UnbindTextBoxChange(ComponentId textBoxId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_textBoxCallbacks.erase(textBoxId);
}

namespace {
struct TextBoxTypeRegistration {
    TextBoxTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "TextBox", "Text Box", false,
            WithCommonProperties(CommonInteractiveProperties(), {
                PropertyDescriptor{
                    .name = "text",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Typography,
                    .description = "Contenido actual del campo de texto",
                },
                PropertyDescriptor{
                    .name = "placeholder",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Typography,
                    .description = "Texto atenuado que se muestra cuando el campo está vacío",
                },
                PropertyDescriptor{
                    .name = "isFocused",
                    .defaultValue = PropertyValue(false),
                    .kind = PropertyKind::Boolean,
                    .group = PropertyGroup::Behavior,
                    .description = "Estado inicial de foco de teclado del campo",
                },
                PropertyDescriptor{
                    .name = "caretIndex",
                    .defaultValue = PropertyValue(0.0),
                    .kind = PropertyKind::Number,
                    .group = PropertyGroup::Advanced,
                    .description = "Posición del cursor de edición, gestionada en tiempo de "
                                   "ejecución por TextBoxEditingController",
                    .readOnly = true,
                },
                PropertyDescriptor{
                    .name = "selectionAnchor",
                    .defaultValue = PropertyValue(0.0),
                    .kind = PropertyKind::Number,
                    .group = PropertyGroup::Advanced,
                    .description = "Ancla de la selección de texto, gestionada en tiempo de "
                                   "ejecución por TextBoxEditingController",
                    .readOnly = true,
                },
                PropertyDescriptor{
                    .name = "imeComposition",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Advanced,
                    .description = "Texto de composición IME en curso (entrada de métodos de "
                                   "entrada asiáticos), gestionado en tiempo de ejecución",
                    .readOnly = true,
                },
                PropertyDescriptor{
                    .name = "imeCompositionCursor",
                    .defaultValue = PropertyValue(0.0),
                    .kind = PropertyKind::Number,
                    .group = PropertyGroup::Advanced,
                    .description = "Posición del cursor dentro de la composición IME en curso, "
                                   "gestionada en tiempo de ejecución",
                    .readOnly = true,
                },
            }),
        });
    }
};
static TextBoxTypeRegistration _textbox_type_registration;
}

}
}
}