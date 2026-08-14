#include "commands/SceneCommandWalker.h"

#include "common/ColorParse.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace avalang {
namespace ui {

namespace {

// Minimal HTML-text escape -- BindingWarningHtml's message text can
// contain a property/identifier name lifted straight from the .avaui
// source (developer-controlled, not end-user input), but escaping it
// anyway costs nothing and keeps the banner from ever being able to
// break out of its own <div>.
std::string EscapeHtmlText(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
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

// Dev-time-only banner rendered directly under a TextBox/ComboBox
// whose BindingWarning() is non-empty (see RenderTree::CheckBindingWarning).
// Absolutely positioned right below the control, same coordinate space
// as everything else this walker emits, so it doesn't shift layout of
// whatever comes after it.
std::string BindingWarningHtml(double x, double y, double w, double h,
                                const std::string& message) {
    return "<div class=\"ava-binding-warning\" style=\"position:absolute; left:" +
           std::to_string(x) + "px; top:" + std::to_string(y + h + 2) +
           "px; width:" + std::to_string(w) +
           "px; box-sizing:border-box; padding:2px 6px; font-size:11px; "
           "line-height:1.3; color:#ffffff; background-color:#dc2626; "
           "border-radius:4px; z-index:9999;\">" +
           EscapeHtmlText(message) + "</div>";
}

bool AncestorIsOverlay(const std::shared_ptr<scene::ISceneNode>& node) {
    auto parent = node ? node->Parent() : nullptr;
    while (parent) {
        const render::IRenderNode* parentRender = parent->GetRenderNode();
        if (parentRender && parentRender->IsOverlay()) {
            return true;
        }
        parent = parent->Parent();
    }
    return false;
}

// Fase 24 -- ScrollView. Nearest ancestor ScrollView's render node, or
// nullptr. Every rect in this codebase is a GLOBAL/absolute coordinate
// (see HTMLRenderer's ".ava-element { position: absolute }" over a
// single flat ".ava-viewport" -- there's no per-parent coordinate
// space). A real `overflow: auto` DOM element introduces its own CSS
// containing block, so anything nested inside one has to have that
// ancestor's own (global) origin subtracted back out, or the browser
// re-applies the wrapper's own on-screen position on top of the
// child's already-absolute coordinates. Only the NEAREST ScrollView
// ancestor matters (not a sum of all of them): its own <div> position
// was already resolved relative to *its* nearest ScrollView ancestor
// by this same function when it was drawn, so subtracting one level
// is correct at any nesting depth. Returns nullptr for nodes with no
// scrolling ancestor -- unchanged, viewport-global behavior.
//
// Stops at the nearest overlay boundary first: an overlay root (e.g. an
// open Dialog) is painted in pass 2 as a top-level sibling of
// #ava-viewport's normal content, never actually nested inside any
// ScrollView's `overflow: auto` <div> in the emitted HTML, however deep
// it sits in the component tree. Walking past that boundary would
// subtract a ScrollView origin the browser never applies to this
// element, shifting it away from its real on-screen position (and, if
// the ScrollView is a full-page one, potentially far enough to look
// like it never opened at all).
const render::IRenderNode* NearestScrollAncestor(const std::shared_ptr<scene::ISceneNode>& node) {
    // An overlay root itself (e.g. an open Dialog) is always painted in
    // Pass 2 as a top-level sibling of #ava-viewport's content, never
    // nested inside any ScrollView's `overflow: auto` <div> -- no matter
    // where it sits in the component tree (see DecomposeDialog's own
    // comment). The walk below only detects "stop, this is an overlay"
    // once it reaches an ANCESTOR's render node, starting at
    // node->Parent() -- so when `node` itself is the overlay root (the
    // Dialog's own drawNode call from drawSubtree(root, isRoot=true)),
    // it skips straight past that check and can return an outer
    // ScrollView instead. That silently subtracted the ScrollView's
    // global origin from the dialog panel's own x/y, shifting the
    // dialog off its real, centered on-screen position -- while its
    // children stayed correct, since for THEM the walk does hit the
    // Dialog's IsOverlay() first and returns nullptr. Checking `node`
    // itself first closes that gap.
    const render::IRenderNode* selfRender = node ? node->GetRenderNode() : nullptr;
    if (selfRender && selfRender->IsOverlay()) {
        return nullptr;
    }
    auto parent = node ? node->Parent() : nullptr;
    while (parent) {
        const render::IRenderNode* parentRender = parent->GetRenderNode();
        if (parentRender && parentRender->IsOverlay()) {
            // Always stop at the overlay boundary -- an overlay's own
            // ancestor ScrollViews (from wherever it sits in the
            // component tree) must never apply to it or its
            // descendants, per the comment above. But if the overlay
            // ITSELF now scrolls its own content (a Dialog capped to a
            // max on-screen height -- see its own dedicated branch in
            // drawNode below, mirroring ScrollView's), it IS the
            // correct, nearest scroll ancestor for anything between
            // here and it: return it instead of nullptr so those
            // descendants get DOM-nested with dialog-local coordinates,
            // exactly like a real ScrollView's children already are.
            if (parentRender->Type() == render::RenderNodeType::Dialog) {
                return parentRender;
            }
            return nullptr;
        }
        if (parentRender && parentRender->Type() == render::RenderNodeType::ScrollView) {
            return parentRender;
        }
        parent = parent->Parent();
    }
    return nullptr;
}

// Fase 24 -- ScrollView. Any node with a scrolling ancestor was already
// (or will be) painted by that ancestor's manual drawSubtree recursion
// below, in DOM-nested order -- the flat scene.ForEachInRenderOrder
// pass must skip it here or it'd get drawn twice, once nested and once
// again as a viewport-global sibling.
bool AncestorIsScrolled(const std::shared_ptr<scene::ISceneNode>& node) {
    return NearestScrollAncestor(node) != nullptr;
}

} // namespace

void SceneCommandWalker::Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer) {
    Walk(scene, sink, renderer, std::string());
}

void SceneCommandWalker::Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                               std::string slotContent) {
    sink.BeginFrame();
    renderer.BeginFrame();

    const bool hasSlotContent = !slotContent.empty();

    std::deque<std::string> textStorage;

    // Draws a single scene node's own commands (no recursion). Shared by
    // the normal painter's-algorithm pass below and the overlay pass --
    // an overlay root's subtree is walked manually since
    // ForEachInRenderOrder gives no subtree control, reusing this exact
    // per-node logic instead of duplicating it.
    auto drawNode = [&](const std::shared_ptr<scene::ISceneNode>& node) {
        if (!node || !node->IsVisible()) {
            return;
        }
        const render::IRenderNode* renderNode = node->GetRenderNode();
        if (!renderNode) {
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Slot) {
            if (hasSlotContent) {
                sink.DrawHtmlFragment(slotContent);
            }
            return;
        }

        const auto rect = renderNode->Rect();
        float x = static_cast<float>(rect.x);
        float y = static_cast<float>(rect.y);
        const float w = static_cast<float>(rect.width);
        const float h = static_cast<float>(rect.height);
        // Fase 24 -- ScrollView. See NearestScrollAncestor's comment: undo
        // the nearest scrolling ancestor's own global origin so this
        // node's DOM position resolves correctly once it's nested inside
        // that ancestor's `overflow: auto` <div> (done by drawSubtree
        // below, not by this flat pass -- that path skips scrolled
        // descendants entirely via AncestorIsScrolled).
        //
        // Gated on SupportsScrollRegions(): this offset only makes sense
        // for a renderer that actually nests children inside a real DOM
        // containing block (HTMLRenderer). A flat-canvas renderer
        // (ImGuiRenderer, GdiRenderer) paints every rect in the same
        // absolute space with no such nesting -- subtracting the
        // ScrollView's own origin there just shifts every descendant off
        // its real on-screen position (usually up/left, out of view),
        // which read as "the ScrollView doesn't render anything".
        if (renderer.SupportsScrollRegions()) {
            if (const render::IRenderNode* scrollAncestor = NearestScrollAncestor(node)) {
                const auto scrollRect = scrollAncestor->Rect();
                x -= static_cast<float>(scrollRect.x);
                y -= static_cast<float>(scrollRect.y);
            }
        }
        const std::string& handler = renderNode->ClickHandler();
        const std::string& cssClass = renderNode->ClassName();

        if (renderNode->Type() == render::RenderNodeType::ScrollView && renderer.SupportsScrollRegions()) {
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{0, 0, 0, 0};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->StrokeWidth())
                                     : 0.0f;
            auto toRgba = [](const Color& c) {
                char buf[40];
                std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.3f)", c.r, c.g, c.b,
                               c.a / 255.0f);
                return std::string(buf);
            };
            // direction "horizontal" scrolls X only; anything else
            // (absent, "vertical", typo) is the vertical default, same
            // soft-fallback convention as IsHorizontalDirection in the
            // layout engine.
            const bool isHorizontal = renderNode->ScrollDirection() == "horizontal";
            std::string html =
                "<div class=\"ava-element ava-scrollview\" style=\"left:" + std::to_string(x) +
                "px; top:" + std::to_string(y) + "px; width:" + std::to_string(w) +
                "px; height:" + std::to_string(h) + "px; overflow-x:" +
                (isHorizontal ? "auto" : "hidden") + "; overflow-y:" +
                (isHorizontal ? "hidden" : "auto") + "; background-color:" +
                toRgba(fillColor) + "; ";
            if (renderNode->ShouldStroke()) {
                html += "border:" + std::to_string(borderWidth) + "px solid " +
                        toRgba(borderColor) + "; ";
            }
            html += "border-radius:" + std::to_string(renderNode->BorderRadius()) + "px;\">";
            // No data-event/data-handler here: 'scroll' isn't one of the
            // DOM event names app.cpp's bridge listens for (only
            // click/change/input, see its avauiEventMap) -- and native
            // `scroll` doesn't bubble to document.body the way those do,
            // so it couldn't reuse that same delegated-listener wiring
            // as-is. IEvent.h's EventType::Scroll / DeltaX/DeltaY stay
            // unwired for now; that's a separate follow-up, not part of
            // making the browser actually scroll this region.
            // Deliberately NOT self-closed -- drawSubtree emits this
            // node's children right after, and the closing "</div>"
            // once they're done, so they land as real DOM children (the
            // only way `overflow: auto` becomes their CSS containing
            // block, see NearestScrollAncestor's comment).
            sink.DrawHtmlFragment(html);
            return;
        }

        // A Dialog's content is fully user-customizable (any children,
        // any amount of text), so its own on-screen height can never be
        // trusted to fit the viewport -- LayoutEngineImpl.cpp sizes a
        // Dialog to its full, uncapped intrinsic content height (see
        // PlaceComponent's isDialog exemption there) so its box and its
        // children's positions stay internally consistent, but nothing
        // stops that intrinsic height from exceeding the screen for a
        // sufficiently long dialog. Rather than let it spill past its
        // own card (the bug this whole Dialog-sizing investigation
        // started from) or run off the top/bottom of the viewport, cap
        // the box's RENDERED height with CSS and make it scroll
        // internally once content exceeds that cap -- mirroring
        // ScrollView's own branch immediately above verbatim (DOM-nest
        // children under this div via NearestScrollAncestor now also
        // recognizing a Dialog as a scroll ancestor, and drawSubtree's
        // matching close-tag below). `max-height: 90vh` is a relative
        // viewport unit, not a value computed from the renderer's own
        // pixel viewport size (no such accessor exists on IRenderer),
        // so it stays correct across window resizes for free. The `top`
        // position was centered assuming the full, uncapped height, so
        // a capped dialog taller than 90vh renders slightly low rather
        // than perfectly centered -- an acceptable trade-off next to a
        // dialog that either runs off-screen or spills its own content
        // past its card.
        //
        // Gated on IsOverlay(): a CLOSED dialog (DecomposeDialog set
        // overlay=false, shouldFill=false, shouldStroke=false, and
        // added no children) must NOT enter this branch -- doing so
        // emitted an empty, transparent <div class="ava-dialog"> in
        // pass 1 at whatever position the layout engine centered the
        // (still-present-in-tree) Dialog node against rootViewport_.
        // That position differs from the OPEN dialog's position
        // (closing the dialog reflows its parent, shifting the
        // rootViewport-centered box), so the closed-state HTML
        // contained an invisible dialog card 16px away from where the
        // open one sat. During the close animation, EventScriptTag's
        // applyHtml() clones the old (open) fragment and fades it out
        // ON TOP of this new (closed) HTML -- the 16px-offset
        // transparent div bled through the fading clone, making the
        // dialog appear to "jump" before disappearing. Skipping this
        // branch for a non-overlay Dialog means the closed dialog
        // emits NOTHING (ShouldFill/ShouldStroke both false, no
        // dedicated type branch matches), and the close clone fades
        // out cleanly over the page content with no ghost div
        // underneath.
        if (renderNode->Type() == render::RenderNodeType::Dialog &&
            renderNode->IsOverlay() &&
            renderer.SupportsScrollRegions()) {
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{0, 0, 0, 0};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->StrokeWidth())
                                     : 0.0f;
            auto toRgba = [](const Color& c) {
                char buf[40];
                std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.3f)", c.r, c.g, c.b,
                               c.a / 255.0f);
                return std::string(buf);
            };
            std::string html =
                "<div class=\"ava-element ava-dialog\" style=\"left:" + std::to_string(x) +
                "px; top:" + std::to_string(y) + "px; width:" + std::to_string(w) +
                "px; height:" + std::to_string(h) + "px; max-height:90vh; overflow-x:hidden; overflow-y:auto; background-color:" +
                toRgba(fillColor) + "; ";
            if (renderNode->ShouldStroke()) {
                html += "border:" + std::to_string(borderWidth) + "px solid " +
                        toRgba(borderColor) + "; ";
            }
            html += "border-radius:" + std::to_string(renderNode->BorderRadius()) + "px;\">";
            // Deliberately NOT self-closed -- see ScrollView's identical
            // comment above; drawSubtree closes this once the dialog's
            // children (now DOM-nested, dialog-local coordinates) are
            // done.
            sink.DrawHtmlFragment(html);
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Button) {
            textStorage.push_back(renderNode->Text());
            textStorage.push_back(renderNode->FontName());
            const std::string& text = textStorage[textStorage.size() - 2];
            const std::string& fontName = textStorage.back();

            Color textColor = common::ParseColor(renderNode->ForegroundColor());
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{0, 0, 0, 0};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->StrokeWidth())
                                     : 0.0f;

            sink.DrawButton(x, y, w, h, text.c_str(),
                             static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                             textColor, fillColor, borderColor, borderWidth,
                             static_cast<float>(renderNode->BorderRadius()),
                             renderNode->Disabled(), handler, cssClass);
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Link) {
            textStorage.push_back(renderNode->Text());
            textStorage.push_back(renderNode->FontName());
            const std::string& text = textStorage[textStorage.size() - 2];
            const std::string& fontName = textStorage.back();

            // Same default a plain <a> gets from every browser's UA
            // stylesheet when no color is set explicitly -- classic
            // "link blue" (#0000EE), not common::ParseColor's generic
            // black fallback (that one's meant for Button/Text, where
            // "no color set" should just mean plain body text). Kept
            // explicit here so all three backends (HTML/ImGui/Gdi)
            // agree on what an unstyled Link looks like, instead of
            // only the browser knowing.
            const std::string& fgColor = renderNode->ForegroundColor();
            Color textColor = fgColor.empty() ? Color{0, 0, 238, 255} : common::ParseColor(fgColor);

            sink.DrawLink(x, y, text.c_str(),
                          static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                          textColor, renderNode->Href(), handler, cssClass);
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Input) {
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{255, 255, 255, 255};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{200, 200, 200, 255};
            auto toHex = [](const Color& c) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
                return std::string(buf);
            };
            std::string html = "<input type=\"text\" class=\"ava-element ava-input\"";
            html += " data-comp-id=\"" + std::to_string(renderNode->Id()) + "\"";
            if (!renderNode->Text().empty()) {
                html += " value=\"" + renderNode->Text() + "\"";
            }
            if (!renderNode->OptionsData().empty()) {
                html += " placeholder=\"" + renderNode->OptionsData() + "\"";
            }
            html += " style=\"position:absolute; left:" + std::to_string(x) +
                    "px; top:" + std::to_string(y) + "px; width:" + std::to_string(w) +
                    "px; height:" + std::to_string(h) + "px; box-sizing:border-box; " +
                    "background-color:" + toHex(fillColor) + "; " +
                    "border:" + std::to_string(renderNode->ShouldStroke() ? renderNode->StrokeWidth() : 1) +
                    "px solid " + toHex(borderColor) + "; " +
                    "font-size:" + std::to_string(renderNode->FontSize()) + "px;\"";
            if (renderNode->Disabled()) {
                html += " disabled";
            }
            // Reuses the existing click-handler wire (ClickHandler(),
            // set from the component's `change` property) as the
            // data-handler target; the JS event bridge already maps
            // `oninput` -> DOM `input` and POSTs {handler, value} back.
            if (!handler.empty()) {
                html += " data-event=\"oninput\" data-handler=\"" + EscapeHtmlText(handler) + "\"";
            }
            html += " />";
            sink.DrawHtmlFragment(html);
            if (!renderNode->BindingWarning().empty()) {
                sink.DrawHtmlFragment(BindingWarningHtml(x, y, w, h, renderNode->BindingWarning()));
            }
            return;
        }

        if (renderNode->ShouldFill() || renderNode->ShouldStroke()) {
            Color fill = renderNode->ShouldFill()
                             ? common::ParseColor(renderNode->BackgroundColor())
                             : Color{0, 0, 0, 0};
            Color border = renderNode->ShouldStroke()
                               ? common::ParseColor(renderNode->BorderColor())
                               : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->StrokeWidth())
                                     : 0.0f;
            if (renderNode->Type() == render::RenderNodeType::Ellipse) {
                const float rx = w / 2.0f;
                const float ry = h / 2.0f;
                sink.DrawEllipse(x + rx, y + ry, rx, ry, fill, border, borderWidth, handler, cssClass);
            } else {
                sink.DrawRectangle(x, y, w, h, fill, border, borderWidth,
                                    static_cast<float>(renderNode->BorderRadius()), handler, cssClass);
            }
        }

        if (renderNode->Type() == render::RenderNodeType::Text ||
            renderNode->Type() == render::RenderNodeType::Label) {
            textStorage.push_back(renderNode->Text());
            textStorage.push_back(renderNode->FontName());
            const std::string& text = textStorage[textStorage.size() - 2];
            const std::string& fontName = textStorage.back();
            Color color = common::ParseColor(renderNode->ForegroundColor());
            float maxWidth = renderNode->Wrap() ? w : -1.0f;
            sink.DrawText(x, y, text.c_str(), static_cast<float>(renderNode->FontSize()),
                          fontName.c_str(), color, handler, cssClass, maxWidth);
        }

        if (renderNode->Type() == render::RenderNodeType::Image ||
            renderNode->Type() == render::RenderNodeType::Icon) {
            textStorage.push_back(renderNode->ImagePath());
            sink.DrawImage(x, y, w, h, textStorage.back().c_str());
        }

        if (renderNode->Type() == render::RenderNodeType::ComboBox) {
            std::string html = "<select class=\"ava-element ava-select\" style=\"position:absolute; left:" +
                                std::to_string(x) + "px; top:" + std::to_string(y) +
                                "px; width:" + std::to_string(w) + "px; height:" + std::to_string(h) + "px;\"";
            html += " data-comp-id=\"" + std::to_string(renderNode->Id()) + "\"";
            if (!handler.empty()) {
                // "onchange" (native `change` event), not "click": a
                // click-mapped handler fires -- and the app.cpp bridge
                // POSTs + swaps the DOM -- the instant the click that
                // opens the native <select> popup completes, which
                // destroys the element mid-interaction and closes the
                // dropdown before the user can pick an option. `change`
                // only fires once an option is actually selected, after
                // the native popup has already closed on its own.
                html += " data-event=\"onchange\" data-handler=\"" + EscapeHtmlText(handler) + "\"";
            }
            html += ">";

            const std::string data = renderNode->OptionsData();
            size_t pos = 0;
            while (pos < data.size()) {
                size_t sep = data.find(";;", pos);
                std::string entry = (sep == std::string::npos) ? data.substr(pos) : data.substr(pos, sep - pos);
                size_t bar1 = entry.find('|');
                size_t bar2 = (bar1 == std::string::npos) ? std::string::npos : entry.find('|', bar1 + 1);
                if (bar1 != std::string::npos && bar2 != std::string::npos) {
                    std::string value = entry.substr(0, bar1);
                    std::string label = entry.substr(bar1 + 1, bar2 - bar1 - 1);
                    bool selected = entry.substr(bar2 + 1) == "1";
                    html += "<option value=\"" + value + "\"" + (selected ? " selected" : "") + ">" + label + "</option>";
                }
                if (sep == std::string::npos) break;
                pos = sep + 2;
            }

            html += "</select>";
            sink.DrawHtmlFragment(html);
        }

        // CheckBox/RadioButton (like TextBox/ComboBox above) can carry a
        // BindingWarning from RenderTree::CheckBindingWarning when their
        // `change` handler has no `isChecked`/`isSelected` state binding
        // to write the new value into -- same dev-time banner, but drawn
        // here generically instead of duplicated per branch, since
        // neither control gets its own dedicated `renderNode->Type()`
        // case above (CheckBox/RadioButton's own box+label children are
        // what actually paint; the parent node itself falls through to
        // here with ShouldFill()/ShouldStroke() both false).
        if (!renderNode->BindingWarning().empty()) {
            sink.DrawHtmlFragment(BindingWarningHtml(x, y, w, h, renderNode->BindingWarning()));
        }
    };

    // Manual recursion, shared by pass 1 (ScrollView roots) and pass 2
    // (overlay roots) below -- both need subtree control the flat
    // scene.ForEachInRenderOrder can't give them (skip-a-subtree for
    // overlays deferred to pass 2; DOM-nest-a-subtree for ScrollView's
    // real `overflow: auto` <div>, closed only once every descendant,
    // however deep, has been drawn).
    //
    // `isRoot` distinguishes the node drawSubtree was originally invoked
    // with from the descendants it recurses into. A nested overlay (e.g.
    // a Dialog several levels inside a ScrollView) must NOT be drawn
    // here even when reached this way -- pass 2 below already owns it
    // (with its own drawSubtree(dialogNode, /*isRoot=*/true) call and
    // its backdrop). Without this check every such Dialog got painted
    // twice -- once inline here with no backdrop at its raw Column-
    // arranged position, once again in pass 2 -- and, worse, its
    // NearestScrollAncestor() lookup treated it as nested inside this
    // ScrollView's own <div> even though pass 2 emits it as a top-level
    // sibling, so its on-screen position came out shifted by this
    // ScrollView's origin. The two bugs together could push a Dialog far
    // enough off its intended spot to look like clicking "open" did
    // nothing at all.
    std::function<void(const std::shared_ptr<scene::ISceneNode>&, bool)> drawSubtree =
        [&](const std::shared_ptr<scene::ISceneNode>& node, bool isRoot) {
            const render::IRenderNode* selfRender = node ? node->GetRenderNode() : nullptr;
            if (!isRoot && selfRender && selfRender->IsOverlay()) {
                return;
            }
            drawNode(node);
            for (const auto& child : node->Children()) {
                drawSubtree(child, false);
            }
            const render::IRenderNode* rn = node ? node->GetRenderNode() : nullptr;
            if (rn && renderer.SupportsScrollRegions() &&
                (rn->Type() == render::RenderNodeType::ScrollView ||
                 rn->Type() == render::RenderNodeType::Dialog)) {
                sink.DrawHtmlFragment("</div>");
            }
        };

    // Pass 1: normal painter's-algorithm order, skipping any node whose
    // render node is marked overlay (or is a descendant of one) -- those
    // are deferred to pass 2 so they paint above everything else. Also
    // skips (and instead hands to drawSubtree, DOM-nested) any
    // ScrollView root and everything already inside one.
    std::vector<std::shared_ptr<scene::ISceneNode>> overlayRoots;

    scene.ForEachInRenderOrder([&](const std::shared_ptr<scene::ISceneNode>& node) {
        if (!node) return;
        const render::IRenderNode* renderNode = node->GetRenderNode();
        if (renderNode && renderNode->IsOverlay()) {
            if (!AncestorIsOverlay(node)) {
                overlayRoots.push_back(node);
            }
            return;
        }
        if (AncestorIsOverlay(node)) {
            return;
        }
        if (AncestorIsScrolled(node)) {
            return;
        }
        if (renderNode && renderNode->Type() == render::RenderNodeType::ScrollView) {
            drawSubtree(node, /*isRoot=*/true);
            return;
        }
        drawNode(node);
    });

    // Pass 2: overlay roots, lowest OverlayPriority() first. Within a
    // single Walk() call, "DOM order alone decides what's on top" holds
    // fine -- painting these last is enough. But a page's overlay can
    // end up spliced into a *different*, outer Walk() call: when the
    // page is rendered inside a layout, ui_pipeline_dynamic_renderer.cpp
    // pulls the "ava-overlay-fragment" block out of the page's own HTML
    // (see ExtractOverlayFragments) and re-inserts it as a raw string at
    // the layout's slot() position -- i.e. wherever the layout puts its
    // page content, which is typically *before* a trailing Footer node
    // in that layout's own DOM/scene tree. That outer walk has no idea
    // this string contains an overlay; it just paints it in tree order
    // like anything else at the slot. Since none of the ancestors this
    // fragment sits inside (the slot itself, the page's own containers)
    // set a CSS z-index, and position:fixed alone doesn't outrank later
    // z-index:auto siblings painted after it in the same stacking
    // context, that Footer -- appearing later in the layout's DOM --
    // paints *on top of* the backdrop instead of being covered by it.
    // An explicit z-index here (not just position:fixed) forces this
    // whole fragment above every ordinary z-index:auto sibling in the
    // nearest real stacking context (.ava-viewport, via its transform),
    // regardless of DOM order -- so it now also covers a layout's
    // trailing Footer, not just whatever happened to be painted earlier.
    std::sort(overlayRoots.begin(), overlayRoots.end(),
              [](const std::shared_ptr<scene::ISceneNode>& a, const std::shared_ptr<scene::ISceneNode>& b) {
                  return a->GetRenderNode()->OverlayPriority() < b->GetRenderNode()->OverlayPriority();
              });

    for (const auto& root : overlayRoots) {
        const render::IRenderNode* renderNode = root->GetRenderNode();
        // `position:relative` (rather than `fixed`) so this wrapper's
        // own box stays exactly where it naturally falls in flow --
        // it must NOT become a new containing block that shifts the
        // dialog's own position:absolute box (see
        // ui_pipeline_dynamic_renderer.cpp's slot-offset comment for
        // why that offset matters). `z-index` alone is enough to lift
        // it, and everything painted inside it, above the rest of the
        // page -- see the comment on Pass 2 above for why that's
        // necessary. Also still a marker wrapper for
        // ExtractOverlayFragments, as before.
        // `data-dialog-id` is `renderNode->Id()` (the source component's
        // stable ComponentId, unrelated to any user `id = "..."`
        // property) -- not used for CSS or layout, only so
        // EventScriptTag's applyHtml() (app.cpp) can tell "this overlay
        // is still open in the new render" from "this overlay just
        // closed" by diffing ids between the old and new DOM, since a
        // closed dialog's fragment isn't in the new HTML at all to
        // diff against structurally.
        sink.DrawHtmlFragment(
            "<div class=\"ava-overlay-fragment\" data-dialog-id=\"" +
            std::to_string(renderNode ? renderNode->Id() : 0) +
            "\" style=\"position:relative; z-index:2147483647;\">");
        if (renderNode && renderNode->HasBackdrop()) {
            sink.DrawHtmlFragment(
                "<div class=\"ava-overlay-backdrop\" style=\"position:fixed; inset:0; "
                "background:rgba(0,0,0,0.65);\"></div>");
        }
        drawSubtree(root, /*isRoot=*/true);
        sink.DrawHtmlFragment("</div>");
    }

    renderer.ProcessCommands(sink.GetCommands());

    sink.EndFrame();
    renderer.EndFrame();
}

} // namespace ui
} // namespace avalang
