#include "commands/RenderCommandSink.h"
#include <cstring>

namespace avalang {
namespace ui {

RenderCommandSink::RenderCommandSink() = default;

void RenderCommandSink::Emit(const RenderCommand& cmd) {
    commands_.push_back(cmd);
}

void RenderCommandSink::BeginFrame() {
    commands_.clear();
    while (!clipStack_.empty()) {
        clipStack_.pop();
    }
}

void RenderCommandSink::EndFrame() {
}

void RenderCommandSink::PushClipRect(float x, float y, float width, float height) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::PushClip;
    clipStack_.push(ClipRect{x, y, width, height, true});
    Emit(cmd);
}

void RenderCommandSink::PopClipRect() {
    if (!clipStack_.empty()) {
        clipStack_.pop();
    }
    RenderCommand cmd;
    cmd.type = RenderCommandType::PopClip;
    Emit(cmd);
}

void RenderCommandSink::Translate(float x, float y) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Translate;
    cmd.transform.x = x;
    cmd.transform.y = y;
    Emit(cmd);
}

void RenderCommandSink::Scale(float sx, float sy) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Scale;
    cmd.transform.sx = sx;
    cmd.transform.sy = sy;
    Emit(cmd);
}

void RenderCommandSink::Rotate(float angle) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Rotate;
    cmd.transform.angle = angle;
    Emit(cmd);
}

void RenderCommandSink::DrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawRectangle;
    cmd.drawRect.x = x;
    cmd.drawRect.y = y;
    cmd.drawRect.width = width;
    cmd.drawRect.height = height;
    cmd.drawRect.fillColor = fillColor;
    cmd.drawRect.borderColor = borderColor;
    cmd.drawRect.borderWidth = borderWidth;
    cmd.drawRect.borderRadius = borderRadius;
    cmd.drawRect.clickHandler = clickHandler;
    cmd.drawRect.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawEllipse;
    cmd.drawEllipse.cx = cx;
    cmd.drawEllipse.cy = cy;
    cmd.drawEllipse.rx = rx;
    cmd.drawEllipse.ry = ry;
    cmd.drawEllipse.fillColor = fillColor;
    cmd.drawEllipse.borderColor = borderColor;
    cmd.drawEllipse.borderWidth = borderWidth;
    cmd.drawEllipse.clickHandler = clickHandler;
    cmd.drawEllipse.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className,
    float maxWidth
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawText;
    cmd.drawText.x = x;
    cmd.drawText.y = y;
    cmd.drawText.text = text;
    cmd.drawText.fontSize = fontSize;
    cmd.drawText.fontName = fontName;
    cmd.drawText.color = color;
    cmd.drawText.clickHandler = clickHandler;
    cmd.drawText.className = className;
    cmd.drawText.maxWidth = maxWidth;
    Emit(cmd);
}

void RenderCommandSink::DrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawImage;
    cmd.drawImage.x = x;
    cmd.drawImage.y = y;
    cmd.drawImage.width = width;
    cmd.drawImage.height = height;
    cmd.drawImage.imagePath = imagePath;
    Emit(cmd);
}

void RenderCommandSink::DrawHtmlFragment(std::string html) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawHtmlFragment;
    cmd.drawHtml.html = std::move(html);
    Emit(cmd);
}

void RenderCommandSink::DrawButton(
    float x, float y, float width, float height,
    const char* text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawButton;
    cmd.drawButton.x = x;
    cmd.drawButton.y = y;
    cmd.drawButton.width = width;
    cmd.drawButton.height = height;
    cmd.drawButton.text = text;
    cmd.drawButton.fontSize = fontSize;
    cmd.drawButton.fontName = fontName;
    cmd.drawButton.textColor = textColor;
    cmd.drawButton.fillColor = fillColor;
    cmd.drawButton.borderColor = borderColor;
    cmd.drawButton.borderWidth = borderWidth;
    cmd.drawButton.borderRadius = borderRadius;
    cmd.drawButton.disabled = disabled;
    cmd.drawButton.clickHandler = clickHandler;
    cmd.drawButton.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawLink;
    cmd.drawLink.x = x;
    cmd.drawLink.y = y;
    cmd.drawLink.text = text;
    cmd.drawLink.fontSize = fontSize;
    cmd.drawLink.fontName = fontName;
    cmd.drawLink.color = color;
    cmd.drawLink.href = href;
    cmd.drawLink.clickHandler = clickHandler;
    cmd.drawLink.className = className;
    Emit(cmd);
}

std::unique_ptr<IRenderCommandSink> IRenderCommandSink::Create() {
    return std::make_unique<RenderCommandSink>();
}

} // namespace ui
} // namespace avalang
