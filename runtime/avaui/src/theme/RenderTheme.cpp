#include "theme/RenderTheme.h"
#include "components/PropertyValue.h"
#include "layout/FontRegistry.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

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

// Registers a styles.ava `fontName` value into FontRegistry when it's
// a resolved font file (has an extension -- see
// ProjectStyleOverrides.cpp's resolveFontPath, which only rewrites
// path-shaped values, leaving a bare family name like "Segoe UI"
// alone) and returns the name FontRegistry/CSS should look it up
// under. `typeKey` (the lowercased style target, e.g. "button", or
// "*" for the project-wide block) makes the synthetic family label
// unique per style block so a project styling both `button` and
// `text` with different TTFs doesn't collide on one shared name --
// mirrors the same "generate a lookup key, never shown to the user"
// approach ProjectFontOverrides::ParseFontLine takes for app.ava
// `font` lines.
static std::string ResolveStyleFontFamily(const std::string& typeKey,
                                           const std::string& fontNameOrPath) {
    const std::filesystem::path asPath(fontNameOrPath);
    if (!asPath.has_extension()) {
        return fontNameOrPath; // bare display label, nothing to register
    }
    const std::string family = "ProjectStyle:" + typeKey;
    if (!layout::FontRegistry::Instance().HasFont(family)) {
        // Best-effort, same as ResolveFont() above: a missing/invalid
        // file just leaves the family unregistered and this label
        // falls back to default metrics, it never blocks styling.
        layout::FontRegistry::Instance().RegisterFontFile(family, fontNameOrPath);
    }
    return family;
}

// Fills in every property a project's styles.ava (`style *` / `style
// <type>` blocks, see theme/ProjectStyleOverrides.h) set for this
// component's resolved style, but only where the component doesn't
// already have that property from its own .avaui source -- the same
// "only fill empty properties" cascade ApplyTypeDefaults uses for
// theme roles, just one priority tier higher (component-authored >
// styles.ava > theme role default). Applies generically to every
// component type rather than gating by type name: the layout engine
// and renderer already read "padding"/"margin"/"spacing"/"fontSize"/
// "backgroundColor"/etc. as plain properties on any component (see
// LayoutEngineImpl::ComputeIntrinsicSize's ReadEdgeInsets(component,
// "padding") call, which isn't type-gated either), so there's no
// type-specific interpretation to reproduce here the way
// ApplyTypeDefaults has to for theme *roles* (which really are
// per-type: a Button's "button" font role isn't a Text's "body" role).
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

// Splits a `style = "primary large"` property value on whitespace
// into lowercased tokens, in the order given (order matters --
// RenderTheme::ApplyToComponent resolves each token left to right,
// see its doc comment). Empty/blank input yields an empty vector,
// same "nothing to apply" no-op every other optional field here
// falls back to.
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

// Apply defaults based on component type
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
        // The root node fills the browser viewport 100% wide/tall
        // (LayoutEngineImpl::Compute stretches it against `available`)
        // -- giving it containerPaddingPx here would eat into that
        // 100% with an unrequested inset, which is exactly the
        // "page has a margin" bug this guards against. Nested
        // containers still get the default so they don't render
        // flush against their parent's edges; only the root is
        // exempted, and only when the author hasn't set "padding"
        // explicitly in their own style block (GetProperty check
        // above still applies to root too).
        if (!isRoot && !comp->GetProperty("padding")) {
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

    // A component's own `style = "..."` property pulls in project-
    // declared style block(s) BEFORE the plain `style <type>` default
    // below -- same idea as an Avalonia/XAML `Style="{StaticResource
    // ...}"` winning over the implicit per-TargetType style. It's one
    // property doing double duty, matching whichever syntax the value
    // names (see ProjectStyleOverrides.h):
    //   - one or more space-separated tokens, each resolved LEFT TO
    //     RIGHT as a class-scoped `style <type>.<token>` block first
    //     (a later token wins over an earlier one for the same field,
    //     same precedence CSS gives `class="a b"`) --
    //   - a token that isn't a declared class for this type but IS a
    //     standalone named `style "token"` block instead resolves
    //     against THAT (an entire look pulled in by name, not scoped
    //     to any type) --
    //   - a token matching neither is simply not recognized and left
    //     alone (e.g. Button's own built-in "primary"/"secondary"
    //     default, when a project hasn't declared a `style
    //     button.primary` or `style "primary"` block).
    // Deliberately ONE property rather than a web-flavored `class`/
    // `styleClass` split: the language targets more than the web
    // renderer, and `style` already reads the same way the
    // styles.ava keyword that declares these blocks does.
    // (via ApplyProjectStyle's own `!comp->GetProperty(...)` guards)
    // any field a token's block sets is already taken by the time the
    // type default is resolved and gets skipped there -- and
    // component-authored properties (anything other than `style`
    // itself, set directly in the .avaui source) still win over all
    // of it.
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

    // styles.ava overrides (component-authored properties still win --
    // see ApplyProjectStyle's own GetProperty guards) take priority
    // over the theme's per-role defaults applied right after.
    if (styles && styles->HasAnyStyles()) {
        ApplyProjectStyle(component, typeKey, styles->Resolve(typeKey));
    }

    // Apply theme to this component
    ApplyTypeDefaults(component, theme, isRoot);

    // Recursively apply to children
    std::vector<IComponent*> children = component->Children();
    for (IComponent* child : children) {
        if (child && !ApplyToComponent(child, theme, styles)) {
            return false;
        }
    }

    return true;
}

} // namespace avalang::ui
