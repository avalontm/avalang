#pragma once
// AvaHost.Rendering.Html -- Component Tree -> HTML. Pages never emit
// HTML directly (plan section "Component Driven"); this is the only
// place that knows how an AvaComponent maps to markup.
#include <string>

#include "avalang.h"

namespace avahost {

struct RenderOptions {
    // Used only when the page's own `metadata` block doesn't declare
    // a `title` (docs/architecture/17_AVAUI_FILE_FORMAT.md,
    // "metadata"). When it does, the page's title -- and its
    // description/image/url/siteName/ogType/twitterCard, if set --
    // always win; see BuildPageMeta in html_renderer.cpp for the full
    // list of recognized keys and the <meta> tags each one produces.
    std::string title = "AvaHost App";
    std::string extraHead;    // raw HTML injected into <head>, e.g. <link> tags
    std::string extraBodyEnd; // raw HTML injected right before </body>, e.g. <script> tags
};

class HtmlRenderer {
public:
    // Renders a full standalone HTML document (doctype, head, body) for
    // the given tree's root component.
    std::string RenderDocument(AvaComponentTree* tree, const RenderOptions& options) const;

    // Renders `layoutTree` as the document, substituting `pageTree`'s
    // rendered HTML wherever a `slot` component appears in the layout
    // (docs/architecture/17_AVAUI_FILE_FORMAT.md, "extends"). If
    // layoutTree is null (extends named a layout that doesn't exist),
    // falls back to rendering pageTree alone -- a bad `extends` never
    // blanks the page.
    std::string RenderDocumentWithLayout(AvaComponentTree* pageTree, AvaComponentTree* layoutTree,
                                          const RenderOptions& options) const;

    // Renders just the fragment for one component subtree (used
    // recursively, and reusable by hot-reload partial updates later).
    std::string RenderComponent(AvaComponent* component) const;

private:
    // slotContent is non-null only while rendering a layout tree
    // (RenderDocumentWithLayout) -- a `slot` component then emits
    // *slotContent verbatim instead of recursing its own (normally
    // empty) children. Threaded through so `slot` can appear nested
    // inside the layout's containers, not just at the top level.
    std::string RenderComponentImpl(AvaComponent* component, const std::string* slotContent) const;
    std::string RenderChildren(AvaComponent* component, const std::string* slotContent) const;
    std::string RenderAttributes(AvaComponent* component) const;
};

} // namespace avahost
