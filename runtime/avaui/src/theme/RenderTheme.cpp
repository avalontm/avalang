#include "theme/RenderTheme.h"
#include "components/PropertyValue.h"
#include "layout/FontRegistry.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace avalang::ui {

static std::string Lowercase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

static ThemeFont ResolveFont(ITheme* theme, const std::string& role) {
    ThemeFont font = theme->Font(role);
    if (!font.filePath.empty() && !layout::FontRegistry::Instance().HasFont(font.name)) {
        layout::FontRegistry::Instance().RegisterFontFile(font.name, font.filePath);
    }
    return font;
}

static std::string ResolveStyleFontFamily(const std::string& typeKey,
                                           const std::string& fontNameOrPath) {
    const std::filesystem::path asPath(fontNameOrPath);
    if (!asPath.has_extension()) {
        return fontNameOrPath; 
    }
    const std::string family = "ProjectStyle:" + typeKey;
    if (!layout::FontRegistry::Instance().HasFont(family)) {
        layout::FontRegistry::Instance().RegisterFontFile(family, fontNameOrPath);
    }
    return family;
}

static void ApplyProjectStyle(IComponent* comp, const std::string& typeKey,
                               const theme::ControlStyleOverride& style) {
    if (style.backgroundColor && !comp->GetProperty("backgroundColor")) {
        comp->SetProperty("backgroundColor", PropertyValue(*style.backgroundColor));
    }
    if (style.textColor && !comp->GetProperty("textColor")) {
        comp->SetProperty("textColor", PropertyValue(*style.textColor));
    }
    if (style.borderColor && !comp->GetProperty("borderColor")) {
        comp->SetProperty("borderColor", PropertyValue(*style.borderColor));
    }
    if (style.fontName && !comp->GetProperty("fontName")) {
        const std::string family = ResolveStyleFontFamily(typeKey, *style.fontName);
        comp->SetProperty("fontName", PropertyValue(family));
    }
    if (style.fontSize && !comp->GetProperty("fontSize")) {
        comp->SetProperty("fontSize", PropertyValue(*style.fontSize));
    }
    if (style.borderWidth && !comp->GetProperty("borderWidth")) {
        comp->SetProperty("borderWidth", PropertyValue(*style.borderWidth));
    }
    if (style.borderRadius && !comp->GetProperty("borderRadius")) {
        comp->SetProperty("borderRadius", PropertyValue(*style.borderRadius));
    }
    if (style.padding && !comp->GetProperty("padding")) {
        comp->SetProperty("padding", PropertyValue(*style.padding));
    }
    if (style.margin && !comp->GetProperty("margin")) {
        comp->SetProperty("margin", PropertyValue(*style.margin));
    }
    if (style.spacing && !comp->GetProperty("spacing")) {
        comp->SetProperty("spacing", PropertyValue(*style.spacing));
    }
}

static std::vector<std::string> SplitStyleClassTokens(const std::string& raw) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : raw) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(Lowercase(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(Lowercase(current));
    }
    return tokens;
}

// Pure layout wrappers (row/column/stack/hstack/vstack/flex) have no card of
// their own -- they exist to position children, not to be seen. `container`
// is the explicit "this is a card" component and keeps its surface/margin
// defaults. Grid/scrollview/page also keep current behavior; they weren't
// part of the bug being fixed here.
static bool IsPureLayoutType(const std::string& type) {
    return type == "row" || type == "column" || type == "stack" ||
           type == "hstack" || type == "vstack" || type == "flex";
}

static void ApplyTypeDefaults(IComponent* comp, ITheme* theme, bool isRoot) {
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
            comp->SetProperty("textColor", PropertyValue("FFFFFF"));
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

    if (type == "image") {
        if (!comp->GetProperty("borderRadius")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("borderRadius", PropertyValue(static_cast<double>(spacing.borderRadiusPx)));
        }
    }

    if (type == "container" || type == "row" || type == "column" || 
        type == "stack" || type == "hstack" || type == "vstack" ||
        type == "scrollview" || type == "scroll" || type == "flex" ||
        type == "grid" || type == "page") {
        if (!comp->GetProperty("backgroundColor") &&
            (type == "container" || type == "scrollview" || type == "scroll")) {
            auto color = theme->Color("surface");
            comp->SetProperty("backgroundColor", PropertyValue(color.hex));
        }

        if (!isRoot && !comp->GetProperty("padding")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("padding", PropertyValue(static_cast<double>(spacing.containerPaddingPx)));
        }
    }

    if (type == "row" || type == "column" || type == "flex" || type == "grid" ||
        type == "scrollview" || type == "scroll") {
        if (!comp->GetProperty("spacing")) {
            auto spacing = theme->Spacing();
            comp->SetProperty("spacing", PropertyValue(static_cast<double>(spacing.containerGapPx)));
        }
    }

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

bool RenderTheme::Apply(ComponentTree* tree, ITheme* theme, const theme::ProjectStyleSheet* styles) {
    if (!tree || !theme) {
        return false;
    }

    IComponent* root = tree->Root();
    if (!root) {
        return false;
    }

    return ApplyToComponent(root, theme, styles, /*isRoot=*/true);
}

bool RenderTheme::ApplyToComponent(IComponent* component, ITheme* theme,
                                    const theme::ProjectStyleSheet* styles, bool isRoot) {
    if (!component || !theme) {
        return false;
    }

    const std::string typeKey = Lowercase(component->TypeName());

    if (styles) {
        if (const auto* styleProp = component->GetProperty("style")) {
            if (styleProp->Type() == PropertyType::String) {
                for (const std::string& token : SplitStyleClassTokens(styleProp->AsString())) {
                    if (styles->HasNamedStyle(token)) {
                        ApplyProjectStyle(component, "named:" + token, styles->ResolveNamed(token));
                    } else {
                        ApplyProjectStyle(component, "class:" + typeKey + "." + token,
                                           styles->ResolveClasses(typeKey, {token}));
                    }
                }
            }
        }
    }

    if (styles && styles->HasAnyStyles()) {
        ApplyProjectStyle(component, typeKey,
                           styles->Resolve(typeKey, IsPureLayoutType(typeKey)));
    }

    ApplyTypeDefaults(component, theme, isRoot);

    std::vector<IComponent*> children = component->Children();
    for (IComponent* child : children) {
        if (child && !ApplyToComponent(child, theme, styles)) {
            return false;
        }
    }

    return true;
}

} // namespace avalang::ui
