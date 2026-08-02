#include "theme/RenderTheme.h"
#include "components/PropertyValue.h"
#include <algorithm>
#include <cctype>

namespace avalang::ui {

// Helper to lowercase strings for case-insensitive comparison
static std::string Lowercase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Apply defaults based on component type
static void ApplyTypeDefaults(IComponent* comp, ITheme* theme) {
    std::string type = Lowercase(comp->TypeName());

    // Button
    if (type == "button") {
        if (!comp->GetProperty("backgroundColor")) {
            auto color = theme->Color("buttonPrimary");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("button");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = theme->Font("button");
            comp->SetProperty("fontName", PropertyValue(font.name));
        }
        if (!comp->GetProperty("textColor")) {
            comp->SetProperty("textColor", PropertyValue("FFFFFF")); // White text on primary
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
        if (!comp->GetProperty("borderRadius")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderRadius", PropertyValue(static_cast<double>(spacing.borderRadiusPx)));
        }
    }

    // Text
    if (type == "text") {
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("body");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = theme->Font("body");
            comp->SetProperty("fontName", PropertyValue(font.name));
        }
        if (!comp->GetProperty("textColor")) {
            auto color = theme->Color("text");
            comp->SetProperty("textColor", PropertyValue(color.hex));
        }
    }

    // Link
    if (type == "link") {
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("link");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = theme->Font("link");
            comp->SetProperty("fontName", PropertyValue(font.name));
        }
        if (!comp->GetProperty("textColor")) {
            auto color = theme->Color("linkDefault");
            comp->SetProperty("textColor", PropertyValue(color.hex));
        }
    }

    // TextBox (input)
    if (type == "textbox" || type == "textinput" || type == "input") {
        if (!comp->GetProperty("backgroundColor")) {
            auto color = theme->Color("inputBackground");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderColor")) {
            auto color = theme->Color("inputBorder");
            comp->SetProperty("borderColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("body");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
    }

    // ComboBox (visualmente un input mas -- mismos 4 campos que TextBox)
    if (type == "combobox") {
        if (!comp->GetProperty("backgroundColor")) {
            auto color = theme->Color("inputBackground");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderColor")) {
            auto color = theme->Color("inputBorder");
            comp->SetProperty("borderColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("body");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
    }

    // Image
    if (type == "image") {
        if (!comp->GetProperty("borderRadius")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderRadius", PropertyValue(static_cast<double>(spacing.borderRadiusPx)));
        }
    }

    // Container/Row/Column (layout containers)
    if (type == "container" || type == "row" || type == "column" || 
        type == "stack" || type == "hstack" || type == "vstack" ||
        type == "scrollview" || type == "scroll") {
        if (!comp->GetProperty("backgroundColor")) {
            auto color = theme->Color("surface");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
    }

    // Label
    if (type == "label") {
        if (!comp->GetProperty("fontSize")) {
            auto font = theme->Font("label");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = theme->Font("label");
            comp->SetProperty("fontName", PropertyValue(font.name));
        }
        if (!comp->GetProperty("textColor")) {
            auto color = theme->Color("text");
            comp->SetProperty("textColor", PropertyValue(color.hex));
        }
    }

    // Checkbox
    if (type == "checkbox") {
        if (!comp->GetProperty("borderColor")) {
            auto color = theme->Color("border");
            comp->SetProperty("borderColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
    }

    // Dialog (modal) -- overlay/backdrop are opt-out defaults, not
    // exclusive to Dialog: any component can set them explicitly, but
    // a dialog that forgets to set overlay=true would just render
    // inline, which is never what "dialog" means.
    if (type == "dialog") {
        if (!comp->GetProperty("overlay")) {
            comp->SetProperty("overlay", PropertyValue(true));
        }
        if (!comp->GetProperty("backdrop")) {
            comp->SetProperty("backdrop", PropertyValue(true));
        }
        if (!comp->GetProperty("backgroundColor")) {
            auto color = theme->Color("surface");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderColor")) {
            auto color = theme->Color("border");
            comp->SetProperty("borderColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
        if (!comp->GetProperty("borderRadius")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderRadius", PropertyValue(static_cast<double>(spacing.borderRadiusPx)));
        }
    }

    // RadioButton
    if (type == "radiobutton" || type == "radio") {
        if (!comp->GetProperty("borderColor")) {
            auto color = theme->Color("border");
            comp->SetProperty("borderColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("borderWidth")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderWidth", PropertyValue(static_cast<double>(spacing.borderWidthPx)));
        }
    }
}

bool RenderTheme::Apply(ComponentTree* tree, ITheme* theme) {
    if (!tree || !theme) {
        return false;
    }

    IComponent* root = tree->Root();
    if (!root) {
        return false;
    }

    return ApplyToComponent(root, theme);
}

bool RenderTheme::ApplyToComponent(IComponent* component, ITheme* theme) {
    if (!component || !theme) {
        return false;
    }

    // Apply theme to this component
    ApplyTypeDefaults(component, theme);

    // Recursively apply to children
    std::vector<IComponent*> children = component->Children();
    for (IComponent* child : children) {
        if (child && !ApplyToComponent(child, theme)) {
            return false;
        }
    }

    return true;
}

} // namespace avalang::ui
