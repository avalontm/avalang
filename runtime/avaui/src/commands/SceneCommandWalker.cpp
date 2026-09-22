#include "commands/SceneCommandWalker.h"

#include "common/ColorParse.h"
#include "theme/VisualState.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace avalang {
namespace ui {

namespace {

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

const render::IRenderNode* NearestScrollAncestor(const std::shared_ptr<scene::ISceneNode>& node) {
    const render::IRenderNode* selfRender = node ? node->GetRenderNode() : nullptr;
    if (selfRender && selfRender->IsOverlay()) {
        return nullptr;
    }
    auto parent = node ? node->Parent() : nullptr;
    while (parent) {
        const render::IRenderNode* parentRender = parent->GetRenderNode();
        if (parentRender && parentRender->IsOverlay()) {
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

bool AncestorIsScrolled(const std::shared_ptr<scene::ISceneNode>& node) {
    return NearestScrollAncestor(node) != nullptr;
}

bool IsComposedControlType(render::RenderNodeType type) {
    return type == render::RenderNodeType::Checkbox || type == render::RenderNodeType::RadioButton;
}

bool AncestorIsComposedControl(const std::shared_ptr<scene::ISceneNode>& node) {
    auto parent = node ? node->Parent() : nullptr;
    while (parent) {
        const render::IRenderNode* parentRender = parent->GetRenderNode();
        if (parentRender && IsComposedControlType(parentRender->Type())) {
            return true;
        }
        parent = parent->Parent();
    }
    return false;
}

bool RectContainsPoint(float x, float y, float w, float h, int px, int py) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

theme::ControlStyleOverride ResolveControlStyle(const InteractiveState* interactive,
                                                 const std::string& typeLower,
                                                 const theme::ComponentVisualState& state) {
    if (!interactive || !interactive->styles) {
        return theme::ControlStyleOverride{};
    }
    return theme::ResolveVisualStyle(*interactive->styles, typeLower, state);
}

// Builtin hover/pressed feedback.
//
// theme::ResolveVisualStyle only returns a non-empty override when the
// *project* has explicitly authored hover/active/focus rules in its own
// style sheet (ProjectStyleSheet::HasAnyStateStyles()). DefaultTheme.cpp
// declares buttonPrimaryHover/buttonPrimaryActive tokens, but nothing
// ever reads them, so a project that never writes its own state styles
// (like most quick prototypes) got a perfectly static button: no visual
// change on hover, no visual change while the mouse is held down, even
// though PointerEnter/PointerLeave/PointerDown/PointerUp are all
// dispatched correctly by EventDispatcher. That's a real gap -- clicking
// should always look like clicking, regardless of whether a `click`
// handler is wired.
//
// This is the built-in fallback: when the project hasn't overridden a
// given color for the current state, darken it a bit (more while
// pressed than while merely hovered). It works for any base color --
// not just the named theme tokens -- so it also covers a raw
// backgroundColor = "0078D4" set directly in a .avaui file.
Color ApplyInteractionTint(const Color& base, bool pressed, bool hovered) {
    if (!pressed && !hovered) {
        return base;
    }
    const float factor = pressed ? 0.72f : 0.88f;
    auto scale = [factor](std::uint8_t channel) -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(static_cast<float>(channel) * factor), 0, 255));
    };
    return Color{scale(base.r), scale(base.g), scale(base.b), base.a};
}

void ApplyBuiltinStateFeedback(const theme::ComponentVisualState& state,
                                const theme::ControlStyleOverride& projectOverride,
                                bool shouldFill, bool shouldStroke,
                                Color& fillColor, Color& borderColor) {
    if (state.disabled || (!state.pressed && !state.hovered)) {
        return;
    }
    if (shouldFill && !projectOverride.backgroundColor) {
        fillColor = ApplyInteractionTint(fillColor, state.pressed, state.hovered);
    }
    if (shouldStroke && !projectOverride.borderColor) {
        borderColor = ApplyInteractionTint(borderColor, state.pressed, state.hovered);
    }
}

}

void SceneCommandWalker::Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                               const InteractiveState* interactive) {
    Walk(scene, sink, renderer, std::string(), interactive);
}

void SceneCommandWalker::Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                               std::string slotContent, const InteractiveState* interactive) {
    sink.BeginFrame();
    renderer.BeginFrame();

    const bool hasSlotContent = !slotContent.empty();

    std::deque<std::string> textStorage;

    auto isFocused = [&](const render::IRenderNode& renderNode) {
        return interactive && interactive->focused != 0 && interactive->focused == renderNode.Id();
    };
    auto isHovered = [&](float hx, float hy, float hw, float hh) {
        return interactive != nullptr &&
               RectContainsPoint(hx, hy, hw, hh, interactive->pointerX, interactive->pointerY);
    };
    auto isPressed = [&](float hx, float hy, float hw, float hh) {
        return interactive != nullptr && interactive->pointerDown &&
               RectContainsPoint(hx, hy, hw, hh, interactive->pointerX, interactive->pointerY);
    };

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
                                     ? static_cast<float>(renderNode->BorderWidth())
                                     : 0.0f;
            auto toRgba = [](const Color& c) {
                char buf[40];
                std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.3f)", c.r, c.g, c.b,
                               c.a / 255.0f);
                return std::string(buf);
            };

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

            sink.DrawHtmlFragment(html);
            return;
        }

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
                                     ? static_cast<float>(renderNode->BorderWidth())
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
                                     ? static_cast<float>(renderNode->BorderWidth())
                                     : 0.0f;
            float borderRadius = static_cast<float>(renderNode->BorderRadius());

            theme::ComponentVisualState state;
            state.disabled = renderNode->Disabled();
            state.pressed = isPressed(x, y, w, h);
            state.focused = isFocused(*renderNode);
            state.hovered = isHovered(x, y, w, h);

            const theme::ControlStyleOverride override =
                ResolveControlStyle(interactive, "button", state);
            if (override.backgroundColor) fillColor = common::ParseColor(*override.backgroundColor);
            if (override.borderColor) borderColor = common::ParseColor(*override.borderColor);
            if (override.textColor) textColor = common::ParseColor(*override.textColor);
            if (override.borderWidth) borderWidth = static_cast<float>(*override.borderWidth);
            if (override.borderRadius) borderRadius = static_cast<float>(*override.borderRadius);
            ApplyBuiltinStateFeedback(state, override, renderNode->ShouldFill(), renderNode->ShouldStroke(),
                                       fillColor, borderColor);

            sink.DrawButton(x, y, w, h, text.c_str(),
                             static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                             textColor, fillColor, borderColor, borderWidth,
                             borderRadius,
                             renderNode->Disabled(), handler, cssClass,
                             renderNode->Id(), "Button");
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Link) {
            textStorage.push_back(renderNode->Text());
            textStorage.push_back(renderNode->FontName());
            const std::string& text = textStorage[textStorage.size() - 2];
            const std::string& fontName = textStorage.back();

            const std::string& fgColor = renderNode->ForegroundColor();
            Color textColor = fgColor.empty() ? Color{0, 0, 238, 255} : common::ParseColor(fgColor);

            theme::ComponentVisualState state;
            state.disabled = renderNode->Disabled();
            state.pressed = isPressed(x, y, w, h);
            state.focused = isFocused(*renderNode);
            state.hovered = isHovered(x, y, w, h);

            const theme::ControlStyleOverride override =
                ResolveControlStyle(interactive, "link", state);
            if (override.textColor) textColor = common::ParseColor(*override.textColor);

            sink.DrawLink(x, y, text.c_str(),
                          static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                          textColor, renderNode->Href(), handler, cssClass,
                          renderNode->Id(), "Link");
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Input) {
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{255, 255, 255, 255};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{200, 200, 200, 255};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->BorderWidth())
                                     : 1.0f;
            Color textColor = common::ParseColor(renderNode->ForegroundColor());
            float borderRadius = static_cast<float>(renderNode->BorderRadius());

            const bool focused = isFocused(*renderNode);
            const bool hovered = isHovered(x, y, w, h);

            theme::ComponentVisualState state;
            state.disabled = renderNode->Disabled();
            state.pressed = isPressed(x, y, w, h);
            state.focused = focused;
            state.hovered = hovered;

            const theme::ControlStyleOverride override =
                ResolveControlStyle(interactive, "input", state);
            if (override.backgroundColor) fillColor = common::ParseColor(*override.backgroundColor);
            if (override.borderColor) borderColor = common::ParseColor(*override.borderColor);
            if (override.textColor) textColor = common::ParseColor(*override.textColor);
            if (override.borderWidth) borderWidth = static_cast<float>(*override.borderWidth);
            if (override.borderRadius) borderRadius = static_cast<float>(*override.borderRadius);
            // Text inputs only get built-in feedback on their border (a
            // hover/focus ring); darkening the fill would make typed text
            // harder to read.
            if (!state.disabled && (state.pressed || state.hovered) &&
                renderNode->ShouldStroke() && !override.borderColor) {
                borderColor = ApplyInteractionTint(borderColor, state.pressed, state.hovered);
            }

            textStorage.push_back(renderNode->FontName());
            const std::string& fontName = textStorage.back();

            const int caretIndex = focused ? renderNode->CaretIndex() : -1;
            const int selectionStart = focused ? renderNode->SelectionStart() : -1;
            const int selectionEnd = focused ? renderNode->SelectionEnd() : -1;

            sink.DrawInput(x, y, w, h, renderNode->Text(), renderNode->OptionsData(),
                           static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                           textColor, fillColor, borderColor, borderWidth,
                           borderRadius,
                           renderNode->Disabled(), focused, hovered,
                           caretIndex, selectionStart, selectionEnd,
                           renderNode->ImeComposition(), renderNode->ImeCompositionCursor(),
                           handler, cssClass,
                           renderNode->Id(), "TextBox");

            if (!renderNode->BindingWarning().empty()) {
                sink.DrawHtmlFragment(BindingWarningHtml(x, y, w, h, renderNode->BindingWarning()));
            }
            return;
        }

        if (IsComposedControlType(renderNode->Type())) {
            const bool isRadio = renderNode->Type() == render::RenderNodeType::RadioButton;

            bool active = false;
            Color boxFillColor{0, 0, 0, 0};
            Color boxBorderColor = common::ParseColor("#666666");
            float borderWidth = 1.0f;
            std::string label;

            for (const auto& child : node->Children()) {
                const render::IRenderNode* childRender = child ? child->GetRenderNode() : nullptr;
                if (!childRender) continue;
                if (childRender->Type() == render::RenderNodeType::Rectangle ||
                    childRender->Type() == render::RenderNodeType::Ellipse) {
                    active = childRender->ShouldFill();
                    boxBorderColor = common::ParseColor(childRender->BorderColor());
                    borderWidth = static_cast<float>(childRender->BorderWidth());
                    if (active) {
                        boxFillColor = common::ParseColor(childRender->BackgroundColor());
                    }
                } else if (childRender->Type() == render::RenderNodeType::Text) {
                    label = childRender->Text();
                }
            }

            Color textColor = common::ParseColor(
                renderNode->ForegroundColor().empty() ? "#000000" : renderNode->ForegroundColor());
            float borderRadius = static_cast<float>(renderNode->BorderRadius());

            textStorage.push_back(renderNode->FontName());
            const std::string& fontName = textStorage.back();

            const bool focused = isFocused(*renderNode);
            const bool hovered = isHovered(x, y, w, h);

            theme::ComponentVisualState state;
            state.disabled = renderNode->Disabled();
            state.pressed = isPressed(x, y, w, h);
            state.focused = focused;
            state.hovered = hovered;
            if (isRadio) {
                state.selected = active;
            } else {
                state.checked = active;
            }

            const theme::ControlStyleOverride override =
                ResolveControlStyle(interactive, isRadio ? "radiobutton" : "checkbox", state);
            if (override.backgroundColor) boxFillColor = common::ParseColor(*override.backgroundColor);
            if (override.borderColor) boxBorderColor = common::ParseColor(*override.borderColor);
            if (override.textColor) textColor = common::ParseColor(*override.textColor);
            if (override.borderWidth) borderWidth = static_cast<float>(*override.borderWidth);
            if (override.borderRadius) borderRadius = static_cast<float>(*override.borderRadius);
            // Border always gets feedback; the fill only if the box is
            // already painted (checked/selected) -- an unchecked box has
            // no fill to darken.
            if (!state.disabled && (state.pressed || state.hovered)) {
                if (active && !override.backgroundColor) {
                    boxFillColor = ApplyInteractionTint(boxFillColor, state.pressed, state.hovered);
                }
                if (!override.borderColor) {
                    boxBorderColor = ApplyInteractionTint(boxBorderColor, state.pressed, state.hovered);
                }
            }

            if (isRadio) {
                sink.DrawRadioButton(x, y, w, h, label,
                                     static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                                     textColor, boxFillColor, boxBorderColor, borderWidth,
                                     active, renderNode->Disabled(), focused, hovered, handler, cssClass,
                                     renderNode->Id(), "RadioButton");
            } else {
                sink.DrawCheckBox(x, y, w, h, label,
                                  static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                                  textColor, boxFillColor, boxBorderColor, borderWidth,
                                  borderRadius,
                                  active, renderNode->Disabled(), focused, hovered, handler, cssClass,
                                  renderNode->Id(), "CheckBox");
            }

            if (!renderNode->BindingWarning().empty()) {
                sink.DrawHtmlFragment(BindingWarningHtml(x, y, w, h, renderNode->BindingWarning()));
            }
            return;
        }

        if (renderNode->Type() == render::RenderNodeType::Path && !renderNode->PathSegments().empty()) {
            Color fill = renderNode->ShouldFill()
                             ? common::ParseColor(renderNode->BackgroundColor())
                             : Color{0, 0, 0, 0};
            Color border = renderNode->ShouldStroke()
                               ? common::ParseColor(renderNode->BorderColor())
                               : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->BorderWidth())
                                     : 0.0f;
            sink.DrawPath(x, y, renderNode->PathSegments(), fill, border, borderWidth,
                          renderNode->PathClosed(), handler, cssClass);
        }

        if (renderNode->Type() != render::RenderNodeType::Path &&
            (renderNode->ShouldFill() || renderNode->ShouldStroke())) {
            Color fill = renderNode->ShouldFill()
                             ? common::ParseColor(renderNode->BackgroundColor())
                             : Color{0, 0, 0, 0};
            Color border = renderNode->ShouldStroke()
                               ? common::ParseColor(renderNode->BorderColor())
                               : Color{0, 0, 0, 0};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->BorderWidth())
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
            bool wrap = renderNode->Wrap();
            float maxWidth = w > 0.0f ? w : -1.0f;
            sink.DrawText(x, y, text.c_str(), static_cast<float>(renderNode->FontSize()),
                          fontName.c_str(), color, handler, cssClass, maxWidth, wrap);
        }

        if (renderNode->Type() == render::RenderNodeType::Image ||
            renderNode->Type() == render::RenderNodeType::Icon) {
            textStorage.push_back(renderNode->ImagePath());
            sink.DrawImage(x, y, w, h, textStorage.back().c_str());
        }

        if (renderNode->Type() == render::RenderNodeType::ComboBox) {
            Color fillColor = renderNode->ShouldFill()
                                   ? common::ParseColor(renderNode->BackgroundColor())
                                   : Color{255, 255, 255, 255};
            Color borderColor = renderNode->ShouldStroke()
                                     ? common::ParseColor(renderNode->BorderColor())
                                     : Color{200, 200, 200, 255};
            float borderWidth = renderNode->ShouldStroke()
                                     ? static_cast<float>(renderNode->BorderWidth())
                                     : 1.0f;
            Color textColor = common::ParseColor(
                renderNode->ForegroundColor().empty() ? "#000000" : renderNode->ForegroundColor());
            float borderRadius = static_cast<float>(renderNode->BorderRadius());

            textStorage.push_back(renderNode->FontName());
            const std::string& fontName = textStorage.back();

            const bool focused = isFocused(*renderNode);
            const bool hovered = isHovered(x, y, w, h);

            theme::ComponentVisualState state;
            state.disabled = renderNode->Disabled();
            state.pressed = isPressed(x, y, w, h);
            state.focused = focused;
            state.hovered = hovered;

            const theme::ControlStyleOverride override =
                ResolveControlStyle(interactive, "combobox", state);
            if (override.backgroundColor) fillColor = common::ParseColor(*override.backgroundColor);
            if (override.borderColor) borderColor = common::ParseColor(*override.borderColor);
            if (override.textColor) textColor = common::ParseColor(*override.textColor);
            if (override.borderWidth) borderWidth = static_cast<float>(*override.borderWidth);
            if (override.borderRadius) borderRadius = static_cast<float>(*override.borderRadius);
            ApplyBuiltinStateFeedback(state, override, renderNode->ShouldFill(), renderNode->ShouldStroke(),
                                       fillColor, borderColor);

            sink.DrawComboBox(x, y, w, h, renderNode->ComboItems(),
                              static_cast<float>(renderNode->FontSize()), fontName.c_str(),
                              textColor, fillColor, borderColor, borderWidth,
                              borderRadius,
                              renderNode->Disabled(), focused, hovered,
                              renderNode->Open(), handler, cssClass,
                              renderNode->Id(), "ComboBox");
        }

        if (!renderNode->BindingWarning().empty()) {
            sink.DrawHtmlFragment(BindingWarningHtml(x, y, w, h, renderNode->BindingWarning()));
        }
    };

    std::function<void(const std::shared_ptr<scene::ISceneNode>&, bool)> drawSubtree =
        [&](const std::shared_ptr<scene::ISceneNode>& node, bool isRoot) {
            const render::IRenderNode* selfRender = node ? node->GetRenderNode() : nullptr;
            if (!isRoot && selfRender && selfRender->IsOverlay()) {
                return;
            }
            drawNode(node);

            const bool isClippedScrollView = selfRender &&
                selfRender->Type() == render::RenderNodeType::ScrollView &&
                !renderer.SupportsScrollRegions();

            if (isClippedScrollView) {
                const auto scrollRect = selfRender->Rect();
                sink.PushClipRect(
                    static_cast<float>(scrollRect.x), static_cast<float>(scrollRect.y),
                    static_cast<float>(scrollRect.width), static_cast<float>(scrollRect.height));
                sink.Translate(
                    static_cast<float>(-selfRender->ScrollOffsetX()),
                    static_cast<float>(-selfRender->ScrollOffsetY()));
            }

            if (!selfRender || !IsComposedControlType(selfRender->Type())) {
                for (const auto& child : node->Children()) {
                    drawSubtree(child, false);
                }
            }

            if (isClippedScrollView) {
                sink.Translate(
                    static_cast<float>(selfRender->ScrollOffsetX()),
                    static_cast<float>(selfRender->ScrollOffsetY()));
                sink.PopClipRect();
            }

            const render::IRenderNode* rn = node ? node->GetRenderNode() : nullptr;
            if (rn && renderer.SupportsScrollRegions() &&
                (rn->Type() == render::RenderNodeType::ScrollView ||
                 rn->Type() == render::RenderNodeType::Dialog)) {
                sink.DrawHtmlFragment("</div>");
            }
        };

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
        if (AncestorIsComposedControl(node)) {
            return;
        }
        if (AncestorIsScrolled(node)) {
            return;
        }
        if (renderNode && renderNode->Type() == render::RenderNodeType::ScrollView) {
            drawSubtree(node, true);
            return;
        }
        drawNode(node);
    });

    std::sort(overlayRoots.begin(), overlayRoots.end(),
              [](const std::shared_ptr<scene::ISceneNode>& a, const std::shared_ptr<scene::ISceneNode>& b) {
                  return a->GetRenderNode()->OverlayPriority() < b->GetRenderNode()->OverlayPriority();
              });

    for (const auto& root : overlayRoots) {
        const render::IRenderNode* renderNode = root->GetRenderNode();
        if (renderer.SupportsScrollRegions()) {
            sink.DrawHtmlFragment(
                "<div class=\"ava-overlay-fragment\" data-dialog-id=\"" +
                std::to_string(renderNode ? renderNode->Id() : 0) +
                "\" style=\"position:relative; z-index:2147483647;\">");
            if (renderNode && renderNode->HasBackdrop()) {
                sink.DrawHtmlFragment(
                    "<div class=\"ava-overlay-backdrop\" style=\"position:fixed; inset:0; "
                    "background:rgba(0,0,0,0.65);\"></div>");
            }
        } else if (renderNode && renderNode->HasBackdrop()) {
            sink.DrawRectangle(0.0f, 0.0f,
                                static_cast<float>(renderer.GetWidth()),
                                static_cast<float>(renderer.GetHeight()),
                                Color{0, 0, 0, 166}, Color{0, 0, 0, 0}, 0.0f);
        }
        drawSubtree(root, true);
        if (renderer.SupportsScrollRegions()) {
            sink.DrawHtmlFragment("</div>");
        }
    }

    renderer.ProcessCommands(sink.GetCommands());

    sink.EndFrame();
    renderer.EndFrame();
}

}
}