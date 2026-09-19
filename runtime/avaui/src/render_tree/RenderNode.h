#pragma once

#include "render_tree/IRenderNode.h"
#include "layout/LayoutTypes.h"

namespace avalang {
namespace ui {
namespace render {

class RenderNode : public IRenderNode {
public:
    RenderNode(ComponentId componentId, RenderNodeType type);
    ~RenderNode() = default;

    RenderNode(const RenderNode&) = delete;
    RenderNode& operator=(const RenderNode&) = delete;

    ComponentId Id() const override { return componentId_; }
    RenderNodeType Type() const override { return type_; }
    void SetType(RenderNodeType t) { type_ = t; }

    const std::vector<std::shared_ptr<IRenderNode>>& Children() const override { return children_; }
    void AddChild(std::shared_ptr<IRenderNode> child) override;
    void RemoveChild(const std::shared_ptr<IRenderNode>& child) override;

    std::string BackgroundColor() const override { return bgColor_; }
    std::string BorderColor() const override { return borderColor_; }
    std::string ForegroundColor() const override { return fgColor_; }

    int BorderWidth() const override { return borderWidth_; }
    int BorderRadius() const override { return borderRadius_; }

    LayoutRect Rect() const override { return rect_; }
    void SetRect(const LayoutRect& r) override { rect_ = r; }

    std::string Text() const override { return text_; }
    std::string ImagePath() const override { return imagePath_; }
    std::string OptionsData() const override { return optionsData_; }
    std::string FontName() const override { return fontName_; }
    int FontSize() const override { return fontSize_; }

    bool ShouldFill() const override { return shouldFill_; }
    bool ShouldStroke() const override { return shouldStroke_; }
    int StrokeWidth() const override { return strokeWidth_; }

    std::string ClickHandler() const override { return clickHandler_; }
    std::string ClassName() const override { return className_; }
    bool Disabled() const override { return disabled_; }
    bool Open() const override { return open_; }
    std::string Href() const override { return href_; }

    bool IsOverlay() const override { return overlay_; }
    bool HasBackdrop() const override { return backdrop_; }
    int OverlayPriority() const override { return overlayPriority_; }
    std::string ScrollDirection() const override { return scrollDirection_; }
    double ScrollOffsetX() const override { return scrollOffsetX_; }
    double ScrollOffsetY() const override { return scrollOffsetY_; }
    std::string BindingWarning() const override { return bindingWarning_; }
    bool Wrap() const override { return wrap_; }

    int CaretIndex() const override { return caretIndex_; }
    int SelectionStart() const override { return selectionStart_; }
    int SelectionEnd() const override { return selectionEnd_; }
    std::string ImeComposition() const override { return imeComposition_; }
    int ImeCompositionCursor() const override { return imeCompositionCursor_; }

    const PathData& PathSegments() const override { return pathSegments_; }
    bool PathClosed() const override { return pathClosed_; }

    const ComboBoxItems& ComboItems() const override { return comboItems_; }
    void SetComboItems(ComboBoxItems items) { comboItems_ = std::move(items); }

    void SetBackgroundColor(const std::string& color) { bgColor_ = color; }
    void SetBorderColor(const std::string& color) { borderColor_ = color; }
    void SetForegroundColor(const std::string& color) { fgColor_ = color; }
    void SetBorderWidth(int w) { borderWidth_ = w; }
    void SetBorderRadius(int r) { borderRadius_ = r; }
    void SetText(const std::string& t) { text_ = t; }
    void SetImagePath(const std::string& p) { imagePath_ = p; }
    void SetOptionsData(const std::string& d) { optionsData_ = d; }
    void SetFontName(const std::string& f) { fontName_ = f; }
    void SetFontSize(int s) { fontSize_ = s; }
    void SetShouldFill(bool f) { shouldFill_ = f; }
    void SetShouldStroke(bool s) { shouldStroke_ = s; }
    void SetStrokeWidth(int w) { strokeWidth_ = w; }
    void SetClickHandler(std::string h) { clickHandler_ = std::move(h); }
    void SetClassName(std::string c) { className_ = std::move(c); }
    void SetDisabled(bool d) { disabled_ = d; }
    void SetOpen(bool o) { open_ = o; }
    void SetHref(std::string h) { href_ = std::move(h); }
    void SetOverlay(bool o) { overlay_ = o; }
    void SetBackdrop(bool b) { backdrop_ = b; }
    void SetOverlayPriority(int p) { overlayPriority_ = p; }
    void SetScrollDirection(std::string d) { scrollDirection_ = std::move(d); }
    void SetScrollOffset(double x, double y) { scrollOffsetX_ = x; scrollOffsetY_ = y; }
    void SetBindingWarning(std::string w) { bindingWarning_ = std::move(w); }
    void SetWrap(bool w) { wrap_ = w; }
    void SetPathSegments(PathData segments) { pathSegments_ = std::move(segments); }
    void SetPathClosed(bool closed) { pathClosed_ = closed; }
    void SetCaretIndex(int index) { caretIndex_ = index; }
    void SetSelectionRange(int start, int end) { selectionStart_ = start; selectionEnd_ = end; }
    void SetImeComposition(std::string text) { imeComposition_ = std::move(text); }
    void SetImeCompositionCursor(int cursor) { imeCompositionCursor_ = cursor; }

private:
    avalang::ui::ComponentId componentId_;
    RenderNodeType type_;
    std::vector<std::shared_ptr<IRenderNode>> children_;
    std::string bgColor_;
    std::string borderColor_;
    std::string fgColor_;
    int borderWidth_ = 0;
    int borderRadius_ = 0;
    std::string text_;
    std::string imagePath_;
    std::string optionsData_;
    std::string fontName_ = "Arial";
    int fontSize_ = 12;
    bool shouldFill_ = false;
    bool shouldStroke_ = false;
    int strokeWidth_ = 1;
    std::string clickHandler_;
    std::string className_;
    bool disabled_ = false;
    bool open_ = false;
    std::string href_;
    bool overlay_ = false;
    bool backdrop_ = false;
    int overlayPriority_ = 0;
    std::string scrollDirection_ = "vertical";
    double scrollOffsetX_ = 0.0;
    double scrollOffsetY_ = 0.0;
    std::string bindingWarning_;
    bool wrap_ = false;
    LayoutRect rect_ = {0, 0, 0, 0};
    PathData pathSegments_;
    bool pathClosed_ = false;
    ComboBoxItems comboItems_;
    int caretIndex_ = -1;
    int selectionStart_ = -1;
    int selectionEnd_ = -1;
    std::string imeComposition_;
    int imeCompositionCursor_ = 0;
};

}
}
}