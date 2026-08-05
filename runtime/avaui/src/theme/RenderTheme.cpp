#include "theme/RenderTheme.h"
#include "components/PropertyValue.h"
#include "layout/FontRegistry.h"
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

// Closes the last gap in "one font file, one source of truth": a
// theme role (button/body/link/label) can point at a project-supplied
// TTF via ThemeFont::filePath (AvaStudio's font picker is meant to
// copy the chosen file into assets/fonts/ and fill this in -- see the
// field's doc comment in ITheme.h). Until now nothing actually read
// filePath; FontRegistry stayed on the built-in default for every
// role regardless of what the theme said, so a project with a custom
// heading font would still get JetBrains Mono metrics. This is called
// everywhere ApplyTypeDefaults resolves a role's ThemeFont, so
// layout measurement, GdiRenderer, and HTMLRenderer all end up
// resolving `font.name` to the SAME bytes the theme configured, not
// just the fallback default.
//
// RegisterFontFile re-reads and re-parses the file on every call, so
// this only pays that cost once per family name per process --
// HasFont(name) short-circuits every subsequent theme lookup for a
// role that resolves to the same already-registered family (the
// common case: the same ThemeFont gets asked for on every component
// of that role).
static ThemeFont ResolveFont(ITheme* theme, const std::string& role) {
    ThemeFont font = theme->Font(role);
    if (!font.filePath.empty() && !layout::FontRegistry::Instance().HasFont(font.name)) {
        // Best-effort: if the file is missing or fails to parse as a
        // TTF/OTF, FontRegistry::RegisterFontFile leaves the previous
        // registration (or the built-in default) in place -- `font`
        // is still returned with its filePath/name as configured, so
        // renderers keep drawing the label the theme asked for even
        // though measurement fell back to the default metrics.
        layout::FontRegistry::Instance().RegisterFontFile(font.name, font.filePath);
    }
    return font;
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
            auto font = ResolveFont(theme, "button");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = ResolveFont(theme, "button");
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
            auto font = ResolveFont(theme, "body");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = ResolveFont(theme, "body");
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
            auto font = ResolveFont(theme, "link");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = ResolveFont(theme, "link");
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
            auto font = ResolveFont(theme, "body");
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
            auto font = ResolveFont(theme, "body");
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
        type == "scrollview" || type == "scroll" || type == "flex" ||
        type == "grid" || type == "page") {
        if (!comp->GetProperty("backgroundColor") &&
            (type == "container" || type == "row" || type == "column" ||
             type == "stack" || type == "hstack" || type == "vstack" ||
             type == "scrollview" || type == "scroll")) {
            auto color = theme->Color("surface");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }
        if (!comp->GetProperty("padding")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("padding", PropertyValue(static_cast<double>(spacing.containerPaddingPx)));
        }
    }

    // Row/Column/Flex/Grid/ScrollView: the only types that actually
    // read "spacing" (see LayoutEngineImpl::ArrangeRowOrColumn/
    // ArrangeGrid) -- filling it in on Stack/Page/Container would be
    // inert, so it's kept as a narrower list than the padding block
    // above.
    if (type == "row" || type == "column" || type == "flex" || type == "grid" ||
        type == "scrollview" || type == "scroll") {
        if (!comp->GetProperty("spacing")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("spacing", PropertyValue(static_cast<double>(spacing.containerGapPx)));
        }
    }

    // Label
    if (type == "label") {
        if (!comp->GetProperty("fontSize")) {
            auto font = ResolveFont(theme, "label");
            comp->SetProperty("fontSize", PropertyValue(static_cast<double>(font.sizePoints)));
        }
        if (!comp->GetProperty("fontName")) {
            auto font = ResolveFont(theme, "label");
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
