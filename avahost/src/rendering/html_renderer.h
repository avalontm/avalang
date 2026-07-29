#pragma once
// AvaHost.Rendering.Html -- Component Tree -> HTML. Pages never emit
// HTML directly (plan section "Component Driven"); this is the only
// place that knows how an AvaComponent maps to markup.
#include <functional>
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

    // Fase 2 module 3 (runtime/state_binder.h): when set, every
    // property's raw source text is passed through this before being
    // rendered, so `value = counter` (a `state` variable) or
    // `"Total: " + counter` shows the current bound/mutated value
    // instead of the literal source text. Left null (the default)
    // renders every property's raw text verbatim -- the same behavior
    // as before state binding existed, so a caller with no VM/state to
    // bind against (e.g. `avahost build`'s static preview, if it never
    // wires one up) doesn't need to special-case anything.
    std::function<std::string(const std::string&)> evalText;
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

    // RenderOptions::evalText for the render currently in progress --
    // set at the top of RenderDocument/RenderDocumentWithLayout (the
    // only two entry points that receive a RenderOptions) and read by
    // every recursive RenderComponentImpl/RenderAttributes call below
    // them, the same threading pattern slotContent already uses for
    // layout substitution. Mutable so the public RenderComponent(...)
    // const entry point (used standalone, e.g. by hot-reload partial
    // updates later) can still run with no evaluator (raw text,
    // unchanged behavior) without needing a non-const overload.
    mutable std::function<std::string(const std::string&)> evalText_;
    std::string EvalText(const std::string& raw) const { return evalText_ ? evalText_(raw) : raw; }
};

} // namespace avahost
