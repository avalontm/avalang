#include "rendering/html_renderer.h"

#include <sstream>
#include <unordered_map>

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
// `metadata` block (docs/architecture/17_AVAUI_FILE_FORMAT.md) --
// that block becomes the root component's properties (avalang.h,
// ava_ui_parse_avaui_text doc comment: "root is a synthetic 'page'
// component whose properties come from the file's `metadata`
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

// Component type (Studio catalog, core/src/ui/registry.cpp) -> CSS
// class used by wwwroot/css/app.css for layout defaults. Compares
// case-insensitively: .avaui source keeps the type exactly as written
// in the file (lowercase by convention, e.g. "column"/"text" -- see
// core/src/ui/avaui_text.cpp's TryParseComponent), while components
// built through the C API directly (e.g. by Ava Studio) may use the
// PascalCase names from ComponentRegistry::KnownTypes() instead.
std::string LayoutClassFor(const std::string& type) {
    std::string t = LowerCopy(type);
    if (t == "column") return "ava-column";
    if (t == "row") return "ava-row";
    if (t == "stack") return "ava-stack";
    if (t == "grid") return "ava-grid";
    if (t == "flex") return "ava-flex";
    if (t == "panel") return "ava-panel";
    if (t == "canvas") return "ava-canvas";
    return "";
}

} // namespace

std::string HtmlRenderer::RenderAttributes(AvaComponent* component) const {
    auto props = ReadProperties(component);
    std::ostringstream attrs;

    const char* id = ava_ui_get_id(component);
    if (id && *id) attrs << " id=\"" << HtmlEscape(id) << "\"";

    std::string type = ava_ui_get_component_type(component) ? ava_ui_get_component_type(component) : "";
    std::string cssClass = LayoutClassFor(type);
    auto classIt = props.find("class");
    if (classIt != props.end() && !classIt->second.empty()) {
        cssClass = cssClass.empty() ? classIt->second : (cssClass + " " + classIt->second);
    }
    if (!cssClass.empty()) attrs << " class=\"" << HtmlEscape(cssClass) << "\"";

    auto styleIt = props.find("style");
    if (styleIt != props.end() && !styleIt->second.empty()) {
        attrs << " style=\"" << HtmlEscape(styleIt->second) << "\"";
    }

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
        out << "<span" << attrs << ">" << HtmlEscape(PropOr(props, "text", PropOr(props, "value"))) << "</span>";
    } else if (typeLower == "button") {
        out << "<button" << attrs << ">" << HtmlEscape(PropOr(props, "text")) << "</button>";
    } else if (typeLower == "image") {
        out << "<img" << attrs << " src=\"" << HtmlEscape(PropOr(props, "src")) << "\" alt=\""
            << HtmlEscape(PropOr(props, "alt")) << "\" />";
    } else if (typeLower == "textbox") {
        out << "<input type=\"text\"" << attrs << " value=\"" << HtmlEscape(PropOr(props, "text")) << "\" />";
    } else if (typeLower == "checkbox") {
        out << "<label" << attrs << "><input type=\"checkbox\""
            << (PropOr(props, "checked") == "true" ? " checked" : "") << " /> "
            << HtmlEscape(PropOr(props, "text")) << "</label>";
    } else if (typeLower == "radiobutton") {
        out << "<label" << attrs << "><input type=\"radio\" name=\""
            << HtmlEscape(PropOr(props, "group")) << "\""
            << (PropOr(props, "checked") == "true" ? " checked" : "") << " /> "
            << HtmlEscape(PropOr(props, "text")) << "</label>";
    } else if (typeLower == "link") {
        out << "<a" << attrs << " href=\"" << HtmlEscape(PropOr(props, "href", "#")) << "\">"
            << HtmlEscape(PropOr(props, "text")) << "</a>";
    } else if (typeLower == "divider") {
        out << "<hr" << attrs << " />";
    } else if (typeLower == "spacer") {
        out << "<div" << attrs << " class=\"ava-spacer\"></div>";
    } else if (typeLower == "video") {
        out << "<video" << attrs << " src=\"" << HtmlEscape(PropOr(props, "src")) << "\" controls></video>";
    } else if (typeLower == "audio") {
        out << "<audio" << attrs << " src=\"" << HtmlEscape(PropOr(props, "src")) << "\" controls></audio>";
    } else if (typeLower == "piechart" || typeLower == "linechart" || typeLower == "barchart") {
        // v0.1 renders a placeholder container; a JS charting library
        // (wwwroot/js) is expected to hydrate elements with this class.
        out << "<div" << attrs << " class=\"ava-chart ava-chart-"
            << HtmlEscape(typeLower) << "\" data-chart-data=\""
            << HtmlEscape(PropOr(props, "data")) << "\"></div>";
    } else if (typeLower == "slot") {
        // Layout placeholder (docs/architecture/17_AVAUI_FILE_FORMAT.md,
        // "extends"). Only meaningful while rendering a layout tree via
        // RenderDocumentWithLayout; a `slot` rendered on its own (no
        // layout in play, or a page mistakenly using the word) just
        // emits nothing rather than an empty <div>.
        if (slotContent) out << *slotContent;
    } else {
        // Containers (page/column/row/stack/grid/flex/panel/canvas) and
        // any unknown/custom type: render as a <div>, recurse children.
        out << "<div" << attrs << ">" << RenderChildren(component, slotContent) << "</div>";
    }

    return out.str();
}

std::string HtmlRenderer::RenderDocument(AvaComponentTree* tree, const RenderOptions& options) const {
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
    AvaComponent* layoutRoot = layoutTree ? ava_ui_get_root(layoutTree) : nullptr;
    if (!layoutRoot) return RenderDocument(pageTree, options); // extends named a missing/empty layout

    AvaComponent* pageRoot = pageTree ? ava_ui_get_root(pageTree) : nullptr;
    std::string pageHtml = pageRoot ? RenderComponent(pageRoot) : "";

    // `metadata` (title/description/image/...) lives on the page --
    // it declares `extends "layout"` alongside its own `metadata`
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
