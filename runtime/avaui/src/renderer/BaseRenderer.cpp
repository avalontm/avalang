#include "renderer/BaseRenderer.h"
#include "commands/RenderCommand.h"
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

void BaseRenderer::ProcessCommands(const std::vector<RenderCommand>& commands) {
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case RenderCommandType::DrawRectangle:
                OnDrawRectangle(
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
                OnDrawEllipse(
                    cmd.drawEllipse.cx, cmd.drawEllipse.cy,
                    cmd.drawEllipse.rx, cmd.drawEllipse.ry,
                    cmd.drawEllipse.fillColor,
                    cmd.drawEllipse.borderColor, cmd.drawEllipse.borderWidth,
                    cmd.drawEllipse.clickHandler,
                    cmd.drawEllipse.className
                );
                break;

            case RenderCommandType::DrawText:
                OnDrawText(
                    cmd.drawText.x, cmd.drawText.y,
                    cmd.drawText.text,
                    cmd.drawText.fontSize, cmd.drawText.fontName,
                    cmd.drawText.color,
                    cmd.drawText.clickHandler,
                    cmd.drawText.className
                );
                break;

            case RenderCommandType::DrawImage:
                OnDrawImage(
                    cmd.drawImage.x, cmd.drawImage.y,
                    cmd.drawImage.width, cmd.drawImage.height,
                    cmd.drawImage.imagePath
                );
                break;

            case RenderCommandType::DrawHtmlFragment:
                OnDrawHtmlFragment(cmd.drawHtml.html);
                break;

            case RenderCommandType::DrawButton:
                OnDrawButton(
                    cmd.drawButton.x, cmd.drawButton.y,
                    cmd.drawButton.width, cmd.drawButton.height,
                    cmd.drawButton.text,
                    cmd.drawButton.fontSize, cmd.drawButton.fontName,
                    cmd.drawButton.textColor,
                    cmd.drawButton.fillColor,
                    cmd.drawButton.borderColor, cmd.drawButton.borderWidth, cmd.drawButton.borderRadius,
                    cmd.drawButton.disabled,
                    cmd.drawButton.clickHandler,
                    cmd.drawButton.className
                );
                break;

            case RenderCommandType::DrawLink:
                OnDrawLink(
                    cmd.drawLink.x, cmd.drawLink.y,
                    cmd.drawLink.text,
                    cmd.drawLink.fontSize, cmd.drawLink.fontName,
                    cmd.drawLink.color,
                    cmd.drawLink.href,
                    cmd.drawLink.clickHandler,
                    cmd.drawLink.className
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
                break;

            case RenderCommandType::PopClip:
                PopClipRect();
                break;
        }
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
    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius, clickHandler, className);
}

void BaseRenderer::DrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    OnDrawEllipse(cx, cy, rx, ry, fillColor, borderColor, borderWidth, clickHandler, className);
}

void BaseRenderer::DrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className
) {
    OnDrawText(x, y, text, fontSize, fontName, color, clickHandler, className);
}

void BaseRenderer::DrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
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
    const std::string& className
) {
    OnDrawButton(x, y, width, height, text, fontSize, fontName, textColor,
                 fillColor, borderColor, borderWidth, borderRadius, disabled,
                 clickHandler, className);
}

void BaseRenderer::DrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className
) {
    OnDrawLink(x, y, text, fontSize, fontName, color, href, clickHandler, className);
}

void BaseRenderer::PushClipRect(float x, float y, float width, float height) {
    clipStack_.push({x, y, width, height});
}

void BaseRenderer::PopClipRect() {
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

} // namespace ui
} // namespace avalang
