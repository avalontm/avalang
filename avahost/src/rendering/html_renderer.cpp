#include "rendering/html_renderer.h"

#include <cctype>
#include <functional>
#include <sstream>
#include <unordered_map>

#include "rendering/event_binder.h"

namespace avahost {

namespace {

// Property value -> std::string. AvaHost properties are always
// Value::String at this layer (display-ready text, verbatim as written
// in the .avaui file -- see core/src/ui/avaui_text.h). ava_string_data's
// AvaVM* parameter is accepted but unused by the C API, so nullptr is
// safe here (same pattern Ava Studio uses in design/avaui_text.cpp).
std::string ValueToString(ava_value_t v) {
    if (v.type != AVA_STRING) return "";
    size_t len = 0;
    const char* data = ava_string_data(nullptr, v, &len);
    if (!data) return "";
    return std::string(data, len);
}

std::string HtmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::unordered_map<std::string, std::string> ReadProperties(AvaComponent* comp) {
    std::unordered_map<std::string, std::string> props;
    size_t count = ava_ui_property_count(comp);
    for (size_t i = 0; i < count; ++i) {
        const char* key = ava_ui_property_key_at(comp, i);
        if (!key) continue;
        std::string keyStr = key;
        ava_value_t value = ava_ui_get_property(comp, keyStr.c_str());
        props[keyStr] = ValueToString(value);
    }
    return props;
}

std::string PropOr(const std::unordered_map<std::string, std::string>& props,
                    const char* key, const std::string& fallback = "") {
    auto it = props.find(key);
    return it != props.end() ? it->second : fallback;
}

// Everything needed for <title> plus the essentials for social-share
// previews (Open Graph + Twitter Card), sourced from the page's own
// `properties` block (docs/architecture/17_AVAUI_FILE_FORMAT.md) --
// that block becomes the root component's properties (avalang.h,
// ava_ui_parse_avaui_text doc comment: "root is a synthetic 'page'
// component whose properties come from the file's `properties`
// block"). Recognized keys, all optional except title:
//   title       -> <title>, og:title, twitter:title
//   description -> <meta name="description">, og:description, twitter:description
//   image       -> og:image, twitter:image (absolute URL recommended --
//                  most crawlers don't resolve a relative one against
//                  your site)
//   url         -> og:url + <link rel="canonical">
//   siteName    -> og:site_name
//   ogType      -> og:type (defaults to "website")
//   twitterCard -> twitter:card (defaults to "summary_large_image" when
//                  `image` is set, otherwise "summary")
struct PageMeta {
    std::string title;
    std::string tags; // rendered <meta>/<link> block, ready to drop into <head>
};

PageMeta BuildPageMeta(AvaComponent* pageRoot, const std::string& fallbackTitle) {
    std::unordered_map<std::string, std::string> props =
        pageRoot ? ReadProperties(pageRoot) : std::unordered_map<std::string, std::string>();

    PageMeta meta;
    meta.title = PropOr(props, "title", fallbackTitle);

    std::string description = PropOr(props, "description");
    std::string image = PropOr(props, "image");
    std::string url = PropOr(props, "url");
    std::string siteName = PropOr(props, "siteName");
    std::string ogType = PropOr(props, "ogType", "website");
    std::string twitterCard = PropOr(props, "twitterCard", image.empty() ? "summary" : "summary_large_image");

    std::ostringstream out;
    if (!description.empty()) {
        out << "<meta name=\"description\" content=\"" << HtmlEscape(description) << "\" />\n";
    }
    out << "<meta property=\"og:type\" content=\"" << HtmlEscape(ogType) << "\" />\n"
        << "<meta property=\"og:title\" content=\"" << HtmlEscape(meta.title) << "\" />\n";
    if (!description.empty()) {
        out << "<meta property=\"og:description\" content=\"" << HtmlEscape(description) << "\" />\n";
    }
    if (!image.empty()) {
        out << "<meta property=\"og:image\" content=\"" << HtmlEscape(image) << "\" />\n";
    }
    if (!url.empty()) {
        out << "<meta property=\"og:url\" content=\"" << HtmlEscape(url) << "\" />\n"
            << "<link rel=\"canonical\" href=\"" << HtmlEscape(url) << "\" />\n";
    }
    if (!siteName.empty()) {
        out << "<meta property=\"og:site_name\" content=\"" << HtmlEscape(siteName) << "\" />\n";
    }
    out << "<meta name=\"twitter:card\" content=\"" << HtmlEscape(twitterCard) << "\" />\n"
        << "<meta name=\"twitter:title\" content=\"" << HtmlEscape(meta.title) << "\" />\n";
    if (!description.empty()) {
        out << "<meta name=\"twitter:description\" content=\"" << HtmlEscape(description) << "\" />\n";
    }
    if (!image.empty()) {
        out << "<meta name=\"twitter:image\" content=\"" << HtmlEscape(image) << "\" />\n";
    }
    meta.tags = out.str();
    return meta;
}

std::string LowerCopy(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return out;
}

// ---------------------------------------------------------------------
// Centralized DSL-property -> CSS translation.
//
// This is the ONLY place in AvaHost that decides how a .avaui property
// becomes visual styling. Anything a component needs to look right in
// the browser must be added HERE (either to TypeDefaultStyle() for
// per-type defaults, or to StyleProps() for per-property rules) instead
// of as a new hardcoded CSS class sprinkled through the renderer with
// its actual definition (if any) living somewhere else entirely. That
// split -- a class name emitted here, its meaning (or lack of one)
// decided elsewhere -- is what caused containers to render with zero
// styling before this change: "ava-column" was emitted on every
// column but never defined in any .css file. Every rule below is
// self-contained: the property (or component type) it reacts to and
// the CSS it produces are declared right next to each other, so a
// missing/wrong style is a one-line diff here, not a scavenger hunt
// across renderer + stylesheet.
//
// Authors can always override this with an explicit `class` (Tailwind
// utilities) or `style` prop on the component -- see RenderAttributes:
// explicit class/style are appended after this generated styling, so
// they win on cascade order.
// ---------------------------------------------------------------------

// Per-component-type default look (docs/architecture/17_AVAUI_FILE_FORMAT.md):
// both the flex/grid base for containers AND the default visual
// appearance for form controls (button, textbox, ...) live in this one
// table. Two kinds of rule, same table, because they're the same
// concern: "what does this component type look like when the author
// hasn't taken over styling with `class`". Compares case-insensitively
// -- .avaui source keeps the type exactly as written in the file
// (lowercase by convention, e.g. "column"/"button" -- see
// core/src/ui/avaui_text.cpp's TryParseComponent), while components
// built through the C API directly (e.g. by Ava Studio) may use the
// PascalCase names from ComponentRegistry::KnownTypes() instead.
//
// Applied ONLY when the component has no explicit `class` (see
// GeneratedStyleFor) -- an inline style always wins the cascade over a
// Tailwind utility class, so generating one unconditionally would make
// `class="bg-blue-600 rounded-full"` on a button silently lose to this
// table's flat default. `class` present means "the author is styling
// this element themselves"; this table then gets out of the way
// entirely for that element, same rule for every type in it, layout or
// not, so there's exactly one thing to remember about how it composes
// with `class`.
const std::unordered_map<std::string, std::string>& TypeDefaultStyle() {
    static const std::unordered_map<std::string, std::string> kTypeDefaultStyle = {
        // Containers: flex/grid base.
        {"column", "display:flex;flex-direction:column;"},
        {"row",    "display:flex;flex-direction:row;"},
        {"stack",  "display:grid;"},                 // children overlap in the same cell
        {"grid",   "display:grid;"},
        {"flex",   "display:flex;"},
        {"panel",  "display:flex;flex-direction:column;"},
        {"canvas", "position:relative;"},
        {"spacer", "flex:1;"},
        // Form controls: without this, Tailwind's preflight reset
        // (loaded by every project via the @tailwindcss/browser CDN
        // script, testproj/app.ava) strips the browser's native button
        // chrome -- background, border, padding -- so a <button> with
        // no `class` renders as bare text, which is what prompted this
        // table.
        {"button", "padding:8px 16px;border:1px solid #d1d5db;border-radius:6px;"
                    "background:#f3f4f6;cursor:pointer;font:inherit;"},
        {"textbox", "padding:6px 10px;border:1px solid #d1d5db;border-radius:6px;font:inherit;"},
    };
    return kTypeDefaultStyle;
}

std::string AsPixels(const std::string& raw) {
    if (raw.empty()) return "";
    // Already has a unit (e.g. "50%", "2rem", "10px") -- pass through
    // verbatim; otherwise treat as a bare number of pixels, the DSL's
    // documented convention (core/src/ui/avaui_text.h's `padding = 20`
    // example).
    char last = raw.back();
    bool hasUnit = !std::isdigit(static_cast<unsigned char>(last)) && last != '.' && last != '-';
    return hasUnit ? raw : (raw + "px");
}

std::string Passthrough(const std::string& raw) { return raw; }

// DSL property key -> CSS declaration it produces. `format` receives
// the property's raw (already state-evaluated) text and returns the
// CSS value; numeric layout properties are treated as pixels
// (AsPixels), everything else (colors, alignment keywords, raw CSS
// lengths) passes through verbatim.
struct StylePropRule {
    const char* cssProperty;
    std::string (*format)(const std::string&);
};

const std::unordered_map<std::string, StylePropRule>& StyleProps() {
    static const std::unordered_map<std::string, StylePropRule> kStyleProps = {
        {"padding",      {"padding", AsPixels}},
        {"margin",       {"margin", AsPixels}},
        {"gap",          {"gap", AsPixels}},
        {"width",        {"width", AsPixels}},
        {"height",       {"height", AsPixels}},
        {"fontSize",     {"font-size", AsPixels}},
        {"radius",       {"border-radius", AsPixels}},
        {"borderRadius", {"border-radius", AsPixels}},
        {"color",        {"color", Passthrough}},
        {"background",   {"background", Passthrough}},
        {"align",        {"align-items", Passthrough}},
        {"justify",      {"justify-content", Passthrough}},
    };
    return kStyleProps;
}

// `fill = "true"` -- the component grows to fill its parent along the
// parent's main axis (docs/architecture/17_AVAUI_FILE_FORMAT.md).
bool IsTrue(const std::string& raw) { return raw == "true" || raw == "1"; }

// Builds the full generated `style="..."` value for a component: the
// per-type default (TypeDefaultStyle, skipped when the author already
// set `class` -- see its header comment) plus every recognized
// property in StyleProps that the component actually sets (always
// applied -- an explicit `padding = 20` is unambiguous author intent
// regardless of `class`), plus `fill`. This is the single function
// that walks both tables above -- callers never need their own
// if/else chain over property names.
std::string GeneratedStyleFor(const std::string& type,
                               const std::unordered_map<std::string, std::string>& props,
                               bool hasExplicitClass,
                               const std::function<std::string(const std::string&)>& evalText) {
    std::string style;

    if (!hasExplicitClass) {
        auto typeIt = TypeDefaultStyle().find(LowerCopy(type));
        if (typeIt != TypeDefaultStyle().end()) style += typeIt->second;
    }

    for (const auto& [key, rule] : StyleProps()) {
        auto it = props.find(key);
        if (it == props.end() || it->second.empty()) continue;
        std::string value = evalText ? evalText(it->second) : it->second;
        if (value.empty()) continue;
        style += std::string(rule.cssProperty) + ":" + rule.format(value) + ";";
    }

    auto fillIt = props.find("fill");
    if (fillIt != props.end() && IsTrue(evalText ? evalText(fillIt->second) : fillIt->second)) {
        style += "flex:1;";
    }

    return style;
}

} // namespace

std::string HtmlRenderer::RenderAttributes(AvaComponent* component) const {
    auto props = ReadProperties(component);
    std::ostringstream attrs;

    const char* id = ava_ui_get_id(component);
    if (id && *id) attrs << " id=\"" << HtmlEscape(id) << "\"";

    // Author-provided `class` (Tailwind utilities, per app.css's
    // documented convention) is the only source of CSS classes --
    // AvaHost never invents class names of its own (see
    // GeneratedStyleFor's header comment for why: a class emitted here
    // with its definition living, or not living, elsewhere was the
    // root cause of unstyled containers).
    std::string type = ava_ui_get_component_type(component) ? ava_ui_get_component_type(component) : "";
    auto classIt = props.find("class");
    std::string classText = (classIt != props.end() && !classIt->second.empty())
                                 ? EvalText(classIt->second)
                                 : "";
    if (!classText.empty()) attrs << " class=\"" << HtmlEscape(classText) << "\"";

    // `style` = generated declarations (per-type default, see
    // GeneratedStyleFor -- skipped when `classText` is non-empty)
    // followed by whatever the author wrote in an explicit `style`
    // prop. Later declarations win within a single `style` attribute,
    // so an explicit `style` always overrides the generated defaults
    // for any property it also sets, without needing to special-case
    // which properties are "ours" vs "theirs".
    std::function<std::string(const std::string&)> evalFn = [this](const std::string& s) { return EvalText(s); };
    std::string style = GeneratedStyleFor(type, props, !classText.empty(), evalFn);
    auto styleIt = props.find("style");
    if (styleIt != props.end() && !styleIt->second.empty()) {
        std::string explicitStyle = EvalText(styleIt->second);
        if (!explicitStyle.empty()) {
            if (!style.empty() && style.back() != ';') style += ';';
            style += explicitStyle;
        }
    }
    if (!style.empty()) attrs << " style=\"" << HtmlEscape(style) << "\"";

    // Fase 2 module 2 (rendering/event_binder.h): data-event/
    // data-handler attributes for whatever `click`-style events the
    // parser bound on this node (explicit `view` binding or automatic
    // naming-convention binding -- see core/src/ui/avaui_text.cpp's
    // AutoBindEvents). A component with no bound events contributes "".
    attrs << EventBinder::RenderAttributes(component);

    return attrs.str();
}

std::string HtmlRenderer::RenderChildren(AvaComponent* component, const std::string* slotContent) const {
    std::ostringstream out;
    size_t count = ava_ui_child_count(component);
    for (size_t i = 0; i < count; ++i) {
        out << RenderComponentImpl(ava_ui_get_child(component, i), slotContent);
    }
    return out.str();
}

std::string HtmlRenderer::RenderComponent(AvaComponent* component) const {
    return RenderComponentImpl(component, nullptr);
}

std::string HtmlRenderer::RenderComponentImpl(AvaComponent* component, const std::string* slotContent) const {
    if (!component) return "";

    std::string type = ava_ui_get_component_type(component) ? ava_ui_get_component_type(component) : "";
    std::string typeLower = LowerCopy(type);
    auto props = ReadProperties(component);
    std::string attrs = RenderAttributes(component);
    std::ostringstream out;

    // Content-bearing leaf types. .avaui source uses lowercase type
    // keywords (column/text/button/...); components built through the
    // C API directly may use ComponentRegistry's PascalCase names
    // instead (Column/Text/Button/...) -- compare lowercased so both
    // work identically (see LayoutClassFor's comment above).
    if (typeLower == "text" || typeLower == "label") {
        out << "<span" << attrs << ">" << HtmlEscape(EvalText(PropOr(props, "text", PropOr(props, "value"))))
            << "</span>";
    } else if (typeLower == "button") {
        out << "<button" << attrs << ">" << HtmlEscape(EvalText(PropOr(props, "text"))) << "</button>";
    } else if (typeLower == "image") {
        out << "<img" << attrs << " src=\"" << HtmlEscape(EvalText(PropOr(props, "src"))) << "\" alt=\""
            << HtmlEscape(EvalText(PropOr(props, "alt"))) << "\" />";
    } else if (typeLower == "textbox") {
        out << "<input type=\"text\"" << attrs << " value=\"" << HtmlEscape(EvalText(PropOr(props, "text")))
            << "\" />";
    } else if (typeLower == "checkbox") {
        out << "<label" << attrs << "><input type=\"checkbox\""
            << (EvalText(PropOr(props, "checked")) == "true" ? " checked" : "") << " /> "
            << HtmlEscape(EvalText(PropOr(props, "text"))) << "</label>";
    } else if (typeLower == "radiobutton") {
        out << "<label" << attrs << "><input type=\"radio\" name=\""
            << HtmlEscape(PropOr(props, "group")) << "\""
            << (EvalText(PropOr(props, "checked")) == "true" ? " checked" : "") << " /> "
            << HtmlEscape(EvalText(PropOr(props, "text"))) << "</label>";
    } else if (typeLower == "link") {
        out << "<a" << attrs << " href=\"" << HtmlEscape(EvalText(PropOr(props, "href", "#"))) << "\">"
            << HtmlEscape(EvalText(PropOr(props, "text"))) << "</a>";
    } else if (typeLower == "divider") {
        out << "<hr" << attrs << " />";
    } else if (typeLower == "spacer") {
        // Styling (flex:1) comes from LayoutTypeStyle via `attrs` --
        // no separate hardcoded class here (see GeneratedStyleFor).
        out << "<div" << attrs << "></div>";
    } else if (typeLower == "video") {
        out << "<video" << attrs << " src=\"" << HtmlEscape(PropOr(props, "src")) << "\" controls></video>";
    } else if (typeLower == "audio") {
        out << "<audio" << attrs << " src=\"" << HtmlEscape(PropOr(props, "src")) << "\" controls></audio>";
    } else if (typeLower == "piechart" || typeLower == "linechart" || typeLower == "barchart") {
        // v0.1 renders a placeholder container; a JS charting library
        // (wwwroot/js) is expected to hydrate elements with this class.
        // Unlike the layout classes removed above, this one is an
        // intentional JS hook (selector for hydration), not a stand-in
        // for CSS -- it's fine for it to have no stylesheet rule.
        out << "<div" << attrs << " class=\"ava-chart ava-chart-"
            << HtmlEscape(typeLower) << "\" data-chart-data=\""
            << HtmlEscape(PropOr(props, "data")) << "\"></div>";
    } else if (typeLower == "slot") {
        // Layout placeholder (docs/architecture/17_AVAUI_FILE_FORMAT.md,
        // "extends"). Only meaningful while rendering a layout tree via
        // RenderDocumentWithLayout; a `slot` rendered on its own (no
        // layout in play, or a page mistakenly using the word) just
        // emits nothing rather than an empty <div>.
        //
        // `slot` accepts the same properties any other node does
        // (`class`, `style`, `id`, ...) -- written as `slot ... end`
        // instead of the bare one-line form. When present, wrap the
        // page content in a <div> carrying them, so the layout author
        // can style the slot's own box directly (e.g. to make it fill
        // its grid/flex row) instead of adding an extra wrapper column
        // just to hold a class. The bare `slot` form (no attrs) keeps
        // emitting the page content with zero extra markup, exactly as
        // before, so existing layouts render identically.
        if (slotContent) {
            if (attrs.empty()) {
                out << *slotContent;
            } else {
                out << "<div" << attrs << ">" << *slotContent << "</div>";
            }
        }
    } else {
        // Containers (page/column/row/stack/grid/flex/panel/canvas) and
        // any unknown/custom type: render as a <div>, recurse children.
        out << "<div" << attrs << ">" << RenderChildren(component, slotContent) << "</div>";
    }

    return out.str();
}

std::string HtmlRenderer::RenderDocument(AvaComponentTree* tree, const RenderOptions& options) const {
    evalText_ = options.evalText;
    AvaComponent* root = tree ? ava_ui_get_root(tree) : nullptr;
    PageMeta meta = BuildPageMeta(root, options.title);

    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        << "<meta charset=\"utf-8\" />\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
        << "<title>" << HtmlEscape(meta.title) << "</title>\n"
        << meta.tags
        << options.extraHead
        << "</head>\n<body>\n";

    if (root) {
        out << RenderComponent(root);
    }

    out << "\n" << options.extraBodyEnd << "</body>\n</html>\n";
    return out.str();
}

std::string HtmlRenderer::RenderDocumentWithLayout(AvaComponentTree* pageTree, AvaComponentTree* layoutTree,
                                                     const RenderOptions& options) const {
    evalText_ = options.evalText;
    AvaComponent* layoutRoot = layoutTree ? ava_ui_get_root(layoutTree) : nullptr;
    if (!layoutRoot) return RenderDocument(pageTree, options); // extends named a missing/empty layout

    AvaComponent* pageRoot = pageTree ? ava_ui_get_root(pageTree) : nullptr;
    std::string pageHtml = pageRoot ? RenderComponent(pageRoot) : "";

    // `properties` (title/description/image/...) lives on the page --
    // it declares `extends "layout"` alongside its own `properties`
    // block (docs/architecture/17_AVAUI_FILE_FORMAT.md) -- never on the
    // shared layout, which has no page-specific SEO/social metadata of
    // its own.
    PageMeta meta = BuildPageMeta(pageRoot, options.title);

    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        << "<meta charset=\"utf-8\" />\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
        << "<title>" << HtmlEscape(meta.title) << "</title>\n"
        << meta.tags
        << options.extraHead
        << "</head>\n<body>\n"
        << RenderComponentImpl(layoutRoot, &pageHtml)
        << "\n" << options.extraBodyEnd << "</body>\n</html>\n";
    return out.str();
}

} // namespace avahost
