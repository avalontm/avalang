#ifndef AVA_UI_RENDER_IRENDER_NODE_H
#define AVA_UI_RENDER_IRENDER_NODE_H

#include "components/IComponent.h"
#include "layout/LayoutTypes.h"
#include "components/PropertyValue.h"
#include <vector>
#include <memory>
#include <string>

namespace avalang {
namespace ui {
namespace render {

enum class RenderNodeType : unsigned char {
    // Container types
    Container,
    Row,
    Column,
    Stack,

    // Visual primitives
    Rectangle,
    Text,
    Image,
    Path,

    // Control types (built on primitives)
    Button,
    Checkbox,
    Input,
    Label,
    Icon,

    // Custom type (user-defined component)
    Custom,

    Ellipse,
    Slot,
    ComboBox,
    Link,
    Dialog,
    ScrollView,
};

// Base render node — represents a drawable entity.
// Created from Components; shares ComponentId but adds visual properties.
class IRenderNode {
public:
    virtual ~IRenderNode() = default;

    // Identity
    virtual ComponentId Id() const = 0;
    virtual RenderNodeType Type() const = 0;

    // Hierarchy
    virtual const std::vector<std::shared_ptr<IRenderNode>>& Children() const = 0;
    virtual void AddChild(std::shared_ptr<IRenderNode> child) = 0;
    virtual void RemoveChild(const std::shared_ptr<IRenderNode>& child) = 0;

    // Visual properties (read from Component property bag)
    // Color: "#RGB" or "#RRGGBB" or "#RRGGBBAA"
    virtual std::string BackgroundColor() const = 0;
    virtual std::string BorderColor() const = 0;
    virtual std::string ForegroundColor() const = 0;  // text color

    // Border: thickness in pixels
    virtual int BorderWidth() const = 0;
    virtual int BorderRadius() const = 0;

    // Layout-provided geometry (read-only)
    virtual LayoutRect Rect() const = 0;
    virtual void SetRect(const LayoutRect& r) = 0;

    // Type-specific accessors
    virtual std::string Text() const = 0;          // for Text node
    virtual std::string ImagePath() const = 0;     // for Image node
    virtual std::string OptionsData() const = 0;   // for ComboBox node: "value|label;;value|label"
    virtual std::string FontName() const = 0;      // for Text node
    virtual int FontSize() const = 0;              // for Text node (pixels)

    // Draw directives (applied by Renderer)
    virtual bool ShouldFill() const = 0;
    virtual bool ShouldStroke() const = 0;
    virtual int StrokeWidth() const = 0;

    // Fase 20.2.x Gap A -- click handler name (mirrors the `click` property
    // on IComponent, propagated by SceneCommandWalker into RenderCommand so
    // HTMLRenderer can emit data-handler="X"). Empty string == no handler
    // attached (default for every node except those with a `click` property
    // in their source component). Additive to the frozen interface, same
    // pattern as BorderRadius() in Fase 21.
    virtual std::string ClickHandler() const = 0;

    // CSS class list taken from the `class` property on the source
    // component. Empty when the component has no `class` set. Renderers
    // that support CSS (HTMLRenderer) use this to emit a class attribute
    // and defer layout to the stylesheet instead of forcing absolute
    // positioning.
    virtual std::string ClassName() const = 0;

    virtual bool Disabled() const = 0;

    // Navigation target for a Link node (the `href` property on the
    // source component). Empty string == not a Link / no target set.
    // Additive to the frozen interface, same pattern as ClassName().
    virtual std::string Href() const = 0;

    // Universal overlay mechanism (not tied to any single control type).
    // A node with IsOverlay() == true is painted last, above the normal
    // render order, instead of inline where it appears in the tree.
    // HasBackdrop() controls whether a full-viewport dim layer is drawn
    // behind it. OverlayPriority() breaks ties between multiple overlay
    // roots (higher paints later, i.e. on top). Any component can opt in
    // via `overlay`/`backdrop`/`zIndex` properties -- Dialog is just the
    // first consumer. Additive to the frozen interface, same pattern as
    // Href().
    virtual bool IsOverlay() const = 0;
    virtual bool HasBackdrop() const = 0;
    virtual int OverlayPriority() const = 0;

    // ScrollView -- which axis scrolls: "vertical" (default), "horizontal",
    // or "both". Empty/unrecognized falls back to "vertical", same
    // soft-fallback convention as ReadAlignment for layout properties.
    // Meaningless for any node whose Type() isn't ScrollView. Additive to
    // the frozen interface, same pattern as Href()/IsOverlay().
    virtual std::string ScrollDirection() const = 0;

    // Dev-time diagnostic for TextBox/ComboBox: non-empty when the
    // control has a `change` handler wired but its two-way-bound
    // property (`text`/`selectedValue`) either isn't set at all or
    // isn't a bare identifier that resolves against a declared `state`
    // variable -- see ApplyPendingControlValue's comment in
    // ui_pipeline_dynamic_renderer.cpp for why that binding is required
    // for typed/selected values to ever persist across the event
    // round-trip. Empty string == no warning. Renderers that support it
    // (HTMLRenderer, via SceneCommandWalker) surface this inline next to
    // the control instead of failing silently. Additive to the frozen
    // interface, same pattern as ScrollDirection().
    virtual std::string BindingWarning() const = 0;
};

} // namespace render
} // namespace ui
} // namespace avalang

#endif // AVA_UI_RENDER_IRENDER_NODE_H
