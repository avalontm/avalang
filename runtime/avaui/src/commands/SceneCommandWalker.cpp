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
                                     ? static_cast<float>(renderNode->StrokeWidth())
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
            std::string html = "<select class=\"ava-element ava-select\" style=\"position:absolute; left:" +
                                std::to_string(x) + "px; top:" + std::to_string(y) +
                                "px; width:" + std::to_string(w) + "px; height:" + std::to_string(h) + "px;\"";
            html += " data-comp-id=\"" + std::to_string(renderNode->Id()) + "\"";
            if (!handler.empty()) {

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

    std::sort(overlayRoots.begin(), overlayRoots.end(),
              [](const std::shared_ptr<scene::ISceneNode>& a, const std::shared_ptr<scene::ISceneNode>& b) {
                  return a->GetRenderNode()->OverlayPriority() < b->GetRenderNode()->OverlayPriority();
              });

    for (const auto& root : overlayRoots) {
        const render::IRenderNode* renderNode = root->GetRenderNode();
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
