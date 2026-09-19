#include "renderer/BaseRenderer.h"
#include "commands/RenderCommand.h"
#include "resources/ResourceManager.h"

#include <cmath>

namespace avalang {
namespace ui {

BaseRenderer::BaseRenderer(int width, int height)
    : width_(width), height_(height), currentOpacity_(1.0f) {
    currentTransform_ = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
}

void BaseRenderer::BeginFrame() {
    OnBeginFrame();
}

void BaseRenderer::EndFrame() {
    OnEndFrame();
}

void BaseRenderer::SetViewport(int width, int height) {
    width_ = width;
    height_ = height;
}

namespace {

bool SameRectStyle(const RenderCommand& a, const RenderCommand& b) {
    const auto& ra = a.drawRect;
    const auto& rb = b.drawRect;
    return ra.fillColor.r == rb.fillColor.r && ra.fillColor.g == rb.fillColor.g &&
           ra.fillColor.b == rb.fillColor.b && ra.fillColor.a == rb.fillColor.a &&
           ra.borderColor.r == rb.borderColor.r && ra.borderColor.g == rb.borderColor.g &&
           ra.borderColor.b == rb.borderColor.b && ra.borderColor.a == rb.borderColor.a &&
           ra.borderWidth == rb.borderWidth && ra.borderRadius == rb.borderRadius;
}

}

void BaseRenderer::ApplyTransform(float& x, float& y) const {
    x += currentTransform_.tx;
    y += currentTransform_.ty;
}

RectGeometry BaseRenderer::ToRectGeometry(const RenderCommand& cmd) const {
    float x = cmd.drawRect.x;
    float y = cmd.drawRect.y;
    ApplyTransform(x, y);
    return RectGeometry{
        x, y, cmd.drawRect.width, cmd.drawRect.height,
        cmd.drawRect.clickHandler, cmd.drawRect.className
    };
}

void BaseRenderer::ProcessCommands(const std::vector<RenderCommand>& commands) {
    size_t i = 0;
    while (i < commands.size()) {
        if (commands[i].type == RenderCommandType::DrawRectangle) {
            size_t j = i + 1;
            std::vector<RectGeometry> batch;
            batch.push_back(ToRectGeometry(commands[i]));
            while (j < commands.size() && commands[j].type == RenderCommandType::DrawRectangle &&
                   SameRectStyle(commands[i], commands[j])) {
                batch.push_back(ToRectGeometry(commands[j]));
                ++j;
            }

            const auto& style = commands[i].drawRect;
            if (batch.size() > 1) {
                OnDrawRectangleBatch(batch, style.fillColor, style.borderColor,
                                      style.borderWidth, style.borderRadius);
            } else {
                DrawRectangle(style.x, style.y, style.width, style.height, style.fillColor,
                               style.borderColor, style.borderWidth, style.borderRadius,
                               style.clickHandler, style.className);
            }
            i = j;
            continue;
        }

        DispatchCommand(commands[i]);
        ++i;
    }
}

void BaseRenderer::ProcessCommandsIncremental(
    const std::vector<RenderCommand>& commands,
    const std::vector<DirtyRect>& dirtyRegions
) {
    for (const auto& cmd : commands) {
        if (!CommandIntersectsAny(cmd, dirtyRegions)) {
            continue;
        }
        DispatchCommand(cmd);
    }
}

void BaseRenderer::DispatchCommand(const RenderCommand& cmd) {
    switch (cmd.type) {
        case RenderCommandType::DrawRectangle:
            DrawRectangle(
                cmd.drawRect.x, cmd.drawRect.y,
                cmd.drawRect.width, cmd.drawRect.height,
                cmd.drawRect.fillColor,
                cmd.drawRect.borderColor, cmd.drawRect.borderWidth,
                cmd.drawRect.borderRadius,
                cmd.drawRect.clickHandler,
                cmd.drawRect.className
            );
            break;

        case RenderCommandType::DrawEllipse:
            DrawEllipse(
                cmd.drawEllipse.cx, cmd.drawEllipse.cy,
                cmd.drawEllipse.rx, cmd.drawEllipse.ry,
                cmd.drawEllipse.fillColor,
                cmd.drawEllipse.borderColor, cmd.drawEllipse.borderWidth,
                cmd.drawEllipse.clickHandler,
                cmd.drawEllipse.className
            );
            break;

        case RenderCommandType::DrawText:
            DrawText(
                cmd.drawText.x, cmd.drawText.y,
                cmd.drawText.text,
                cmd.drawText.fontSize, cmd.drawText.fontName,
                cmd.drawText.color,
                cmd.drawText.clickHandler,
                cmd.drawText.className,
                cmd.drawText.maxWidth,
                cmd.drawText.wrap
            );
            break;

        case RenderCommandType::DrawImage: {
            std::string resolvedPath = cmd.drawImage.imagePath
                ? ResourceManager::Instance().ResolveImagePath(cmd.drawImage.imagePath)
                : std::string();
            DrawImage(
                cmd.drawImage.x, cmd.drawImage.y,
                cmd.drawImage.width, cmd.drawImage.height,
                resolvedPath.c_str()
            );
            break;
        }

        case RenderCommandType::DrawHtmlFragment:
            OnDrawHtmlFragment(cmd.drawHtml.html);
            break;

        case RenderCommandType::DrawButton:
            DrawButton(
                cmd.drawButton.x, cmd.drawButton.y,
                cmd.drawButton.width, cmd.drawButton.height,
                cmd.drawButton.text,
                cmd.drawButton.fontSize, cmd.drawButton.fontName,
                cmd.drawButton.textColor,
                cmd.drawButton.fillColor,
                cmd.drawButton.borderColor, cmd.drawButton.borderWidth, cmd.drawButton.borderRadius,
                cmd.drawButton.disabled,
                cmd.drawButton.clickHandler,
                cmd.drawButton.className,
                cmd.drawButton.compId,
                cmd.drawButton.avaType
            );
            break;

        case RenderCommandType::DrawLink:
            DrawLink(
                cmd.drawLink.x, cmd.drawLink.y,
                cmd.drawLink.text,
                cmd.drawLink.fontSize, cmd.drawLink.fontName,
                cmd.drawLink.color,
                cmd.drawLink.href,
                cmd.drawLink.clickHandler,
                cmd.drawLink.className,
                cmd.drawLink.compId,
                cmd.drawLink.avaType
            );
            break;

        case RenderCommandType::DrawPath:
            DrawPath(
                cmd.drawPath.x, cmd.drawPath.y,
                cmd.drawPath.segments,
                cmd.drawPath.fillColor,
                cmd.drawPath.borderColor, cmd.drawPath.borderWidth,
                cmd.drawPath.closed,
                cmd.drawPath.clickHandler,
                cmd.drawPath.className
            );
            break;

        case RenderCommandType::DrawInput:
            DrawInput(
                cmd.drawInput.x, cmd.drawInput.y,
                cmd.drawInput.width, cmd.drawInput.height,
                cmd.drawInput.text,
                cmd.drawInput.placeholder,
                cmd.drawInput.fontSize, cmd.drawInput.fontName,
                cmd.drawInput.textColor,
                cmd.drawInput.fillColor,
                cmd.drawInput.borderColor, cmd.drawInput.borderWidth, cmd.drawInput.borderRadius,
                cmd.drawInput.disabled, cmd.drawInput.focused, cmd.drawInput.hovered,
                cmd.drawInput.caretIndex, cmd.drawInput.selectionStart, cmd.drawInput.selectionEnd,
                cmd.drawInput.imeComposition, cmd.drawInput.imeCompositionCursor,
                cmd.drawInput.clickHandler,
                cmd.drawInput.className,
                cmd.drawInput.compId,
                cmd.drawInput.avaType
            );
            break;

        case RenderCommandType::DrawCheckBox:
            DrawCheckBox(
                cmd.drawCheckBox.x, cmd.drawCheckBox.y,
                cmd.drawCheckBox.width, cmd.drawCheckBox.height,
                cmd.drawCheckBox.text,
                cmd.drawCheckBox.fontSize, cmd.drawCheckBox.fontName,
                cmd.drawCheckBox.textColor,
                cmd.drawCheckBox.boxFillColor,
                cmd.drawCheckBox.boxBorderColor, cmd.drawCheckBox.borderWidth, cmd.drawCheckBox.borderRadius,
                cmd.drawCheckBox.checked, cmd.drawCheckBox.disabled, cmd.drawCheckBox.focused, cmd.drawCheckBox.hovered,
                cmd.drawCheckBox.clickHandler,
                cmd.drawCheckBox.className,
                cmd.drawCheckBox.compId,
                cmd.drawCheckBox.avaType
            );
            break;

        case RenderCommandType::DrawRadioButton:
            DrawRadioButton(
                cmd.drawRadioButton.x, cmd.drawRadioButton.y,
                cmd.drawRadioButton.width, cmd.drawRadioButton.height,
                cmd.drawRadioButton.text,
                cmd.drawRadioButton.fontSize, cmd.drawRadioButton.fontName,
                cmd.drawRadioButton.textColor,
                cmd.drawRadioButton.boxFillColor,
                cmd.drawRadioButton.boxBorderColor, cmd.drawRadioButton.borderWidth,
                cmd.drawRadioButton.selected, cmd.drawRadioButton.disabled,
                cmd.drawRadioButton.focused, cmd.drawRadioButton.hovered,
                cmd.drawRadioButton.clickHandler,
                cmd.drawRadioButton.className,
                cmd.drawRadioButton.compId,
                cmd.drawRadioButton.avaType
            );
            break;

        case RenderCommandType::DrawComboBox:
            DrawComboBox(
                cmd.drawComboBox.x, cmd.drawComboBox.y,
                cmd.drawComboBox.width, cmd.drawComboBox.height,
                cmd.drawComboBox.items,
                cmd.drawComboBox.fontSize, cmd.drawComboBox.fontName,
                cmd.drawComboBox.textColor,
                cmd.drawComboBox.fillColor,
                cmd.drawComboBox.borderColor, cmd.drawComboBox.borderWidth, cmd.drawComboBox.borderRadius,
                cmd.drawComboBox.disabled, cmd.drawComboBox.focused, cmd.drawComboBox.hovered, cmd.drawComboBox.open,
                cmd.drawComboBox.clickHandler,
                cmd.drawComboBox.className,
                cmd.drawComboBox.compId,
                cmd.drawComboBox.avaType
            );
            break;

        case RenderCommandType::Translate:
            Translate(cmd.transform.x, cmd.transform.y);
            break;

        case RenderCommandType::Scale:
            Scale(cmd.transform.sx, cmd.transform.sy);
            break;

        case RenderCommandType::Rotate:
            Rotate(cmd.transform.angle);
            break;

        case RenderCommandType::PushClip:
            PushClipRect(cmd.pushClip.x, cmd.pushClip.y, cmd.pushClip.width, cmd.pushClip.height);
            break;

        case RenderCommandType::PopClip:
            PopClipRect();
            break;
    }
}

void BaseRenderer::DrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    ApplyTransform(x, y);
    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius, clickHandler, className);
}

void BaseRenderer::DrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    ApplyTransform(cx, cy);
    OnDrawEllipse(cx, cy, rx, ry, fillColor, borderColor, borderWidth, clickHandler, className);
}

void BaseRenderer::DrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className,
    float maxWidth,
    bool wrap
) {
    ApplyTransform(x, y);
    OnDrawText(x, y, text, fontSize, fontName, color, clickHandler, className, maxWidth, wrap);
}

void BaseRenderer::DrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    ApplyTransform(x, y);
    OnDrawImage(x, y, width, height, imagePath);
}

void BaseRenderer::DrawHtmlFragment(std::string html) {
    OnDrawHtmlFragment(html);
}

void BaseRenderer::DrawButton(
    float x, float y, float width, float height,
    const char* text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawButton(x, y, width, height, text, fontSize, fontName, textColor,
                 fillColor, borderColor, borderWidth, borderRadius, disabled,
                 clickHandler, className, compId, avaType);
}

void BaseRenderer::DrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawLink(x, y, text, fontSize, fontName, color, href, clickHandler, className, compId, avaType);
}

void BaseRenderer::DrawPath(
    float x, float y,
    const std::vector<render::PathSegment>& segments,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    bool closed,
    const std::string& clickHandler,
    const std::string& className
) {
    ApplyTransform(x, y);
    OnDrawPath(x, y, segments, fillColor, borderColor, borderWidth, closed, clickHandler, className);
}

void BaseRenderer::DrawInput(
    float x, float y, float width, float height,
    const std::string& text,
    const std::string& placeholder,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled, bool focused, bool hovered,
    int caretIndex, int selectionStart, int selectionEnd,
    const std::string& imeComposition, int imeCompositionCursor,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawInput(x, y, width, height, text, placeholder, fontSize, fontName, textColor,
                fillColor, borderColor, borderWidth, borderRadius,
                disabled, focused, hovered,
                caretIndex, selectionStart, selectionEnd,
                imeComposition, imeCompositionCursor,
                clickHandler, className, compId, avaType);
}

void BaseRenderer::DrawCheckBox(
    float x, float y, float width, float height,
    const std::string& text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& boxFillColor,
    const Color& boxBorderColor, float borderWidth, float borderRadius,
    bool checked, bool disabled, bool focused, bool hovered,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawCheckBox(x, y, width, height, text, fontSize, fontName, textColor,
                   boxFillColor, boxBorderColor, borderWidth, borderRadius,
                   checked, disabled, focused, hovered, clickHandler, className, compId, avaType);
}

void BaseRenderer::DrawRadioButton(
    float x, float y, float width, float height,
    const std::string& text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& boxFillColor,
    const Color& boxBorderColor, float borderWidth,
    bool selected, bool disabled, bool focused, bool hovered,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawRadioButton(x, y, width, height, text, fontSize, fontName, textColor,
                       boxFillColor, boxBorderColor, borderWidth,
                       selected, disabled, focused, hovered, clickHandler, className, compId, avaType);
}

void BaseRenderer::DrawComboBox(
    float x, float y, float width, float height,
    const std::vector<render::ComboBoxItem>& items,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled, bool focused, bool hovered, bool open,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    ApplyTransform(x, y);
    OnDrawComboBox(x, y, width, height, items, fontSize, fontName, textColor,
                   fillColor, borderColor, borderWidth, borderRadius,
                   disabled, focused, hovered, open, clickHandler, className, compId, avaType);
}

void BaseRenderer::PushClipRect(float x, float y, float width, float height) {
    ApplyTransform(x, y);
    clipStack_.push({x, y, width, height});
    OnPushClipRect(x, y, width, height);
}

void BaseRenderer::PopClipRect() {
    OnPopClipRect();
    if (!clipStack_.empty()) {
        clipStack_.pop();
    }
}

void BaseRenderer::Translate(float x, float y) {
    currentTransform_.tx += x;
    currentTransform_.ty += y;
}

void BaseRenderer::Scale(float sx, float sy) {
    currentTransform_.sx *= sx;
    currentTransform_.sy *= sy;
}

void BaseRenderer::Rotate(float angle) {
    currentTransform_.rotation += angle;
}

void BaseRenderer::PushTransform() {
    transformStack_.push(currentTransform_);
}

void BaseRenderer::PopTransform() {
    if (!transformStack_.empty()) {
        currentTransform_ = transformStack_.top();
        transformStack_.pop();
    }
}

void BaseRenderer::SetOpacity(float opacity) {
    currentOpacity_ = opacity;
    if (currentOpacity_ < 0.0f) currentOpacity_ = 0.0f;
    if (currentOpacity_ > 1.0f) currentOpacity_ = 1.0f;
}

void BaseRenderer::ResetTransform() {
    currentTransform_ = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
    while (!transformStack_.empty()) transformStack_.pop();
    while (!clipStack_.empty()) clipStack_.pop();
    currentOpacity_ = 1.0f;
}

}
}