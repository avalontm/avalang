#pragma once

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "layout/LayoutTypes.h"
#include "render_tree/PathGeometry.h"
#include "render_tree/ComboBoxData.h"

#include <memory>
#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace render {

enum class RenderNodeType : unsigned char {
    Container,
    Row,
    Column,
    Stack,
    Rectangle,
    Text,
    Image,
    Path,
    Button,
    Checkbox,
    Input,
    Label,
    Icon,
    Custom,
    Ellipse,
    Slot,
    ComboBox,
    Link,
    Dialog,
    ScrollView,
    RadioButton,
};

class IRenderNode {
public:
    virtual ~IRenderNode() = default;

    virtual ComponentId Id() const = 0;
    virtual RenderNodeType Type() const = 0;

    virtual const std::vector<std::shared_ptr<IRenderNode>>& Children() const = 0;
    virtual void AddChild(std::shared_ptr<IRenderNode> child) = 0;
    virtual void RemoveChild(const std::shared_ptr<IRenderNode>& child) = 0;

    virtual std::string BackgroundColor() const = 0;
    virtual std::string BorderColor() const = 0;
    virtual std::string ForegroundColor() const = 0;

    virtual int BorderWidth() const = 0;
    virtual int BorderRadius() const = 0;

    virtual LayoutRect Rect() const = 0;
    virtual void SetRect(const LayoutRect& r) = 0;

    virtual std::string Text() const = 0;
    virtual std::string ImagePath() const = 0;
    virtual std::string OptionsData() const = 0;
    virtual std::string FontName() const = 0;
    virtual int FontSize() const = 0;

    virtual bool ShouldFill() const = 0;
    virtual bool ShouldStroke() const = 0;
    virtual int StrokeWidth() const = 0;

    virtual std::string ClickHandler() const = 0;
    virtual std::string ClassName() const = 0;
    virtual bool Disabled() const = 0;
    virtual bool Open() const = 0;
    virtual std::string Href() const = 0;

    virtual bool IsOverlay() const = 0;
    virtual bool HasBackdrop() const = 0;
    virtual int OverlayPriority() const = 0;

    virtual std::string ScrollDirection() const = 0;
    virtual double ScrollOffsetX() const = 0;
    virtual double ScrollOffsetY() const = 0;
    virtual std::string BindingWarning() const = 0;
    virtual bool Wrap() const = 0;

    virtual int CaretIndex() const = 0;
    virtual int SelectionStart() const = 0;
    virtual int SelectionEnd() const = 0;
    virtual std::string ImeComposition() const = 0;
    virtual int ImeCompositionCursor() const = 0;

    virtual const PathData& PathSegments() const = 0;
    virtual bool PathClosed() const = 0;

    virtual const ComboBoxItems& ComboItems() const = 0;
};

}
}
}