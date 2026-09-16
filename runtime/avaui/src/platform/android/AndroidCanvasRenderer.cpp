#include "AndroidCanvasRenderer.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {

AndroidCanvasRenderer::AndroidCanvasRenderer(int width, int height)
    : BaseRenderer(width, height) {
}

uint32_t AndroidCanvasRenderer::ToArgb(const Color& c) {
    return (static_cast<uint32_t>(c.a) << 24) |
           (static_cast<uint32_t>(c.r) << 16) |
           (static_cast<uint32_t>(c.g) << 8) |
           static_cast<uint32_t>(c.b);
}

void AndroidCanvasRenderer::OnDrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string&,
    const std::string&
) {
    platform::android::Bridge_CanvasDrawRect(x, y, width, height,
        ToArgb(fillColor), ToArgb(borderColor), borderWidth, borderRadius);
}

void AndroidCanvasRenderer::OnDrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string&,
    const std::string&
) {
    platform::android::Bridge_CanvasDrawEllipse(cx, cy, rx, ry,
        ToArgb(fillColor), ToArgb(borderColor), borderWidth);
}

void AndroidCanvasRenderer::OnDrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string&,
    const std::string&,
    float maxWidth,
    bool wrap
) {
    platform::android::Bridge_CanvasDrawText(x, y, text ? text : "", fontSize,
        fontName ? fontName : "", ToArgb(color), maxWidth, wrap);
}

void AndroidCanvasRenderer::OnDrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    platform::android::Bridge_CanvasDrawImage(x, y, width, height, imagePath ? imagePath : "");
}

void AndroidCanvasRenderer::OnDrawHtmlFragment(const std::string&) {
}

void AndroidCanvasRenderer::OnDrawButton(
    float x, float y, float width, float height,
    const char* text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled,
    const std::string&,
    const std::string&
) {
    platform::android::Bridge_CanvasDrawButton(x, y, width, height, text ? text : "",
        fontSize, fontName ? fontName : "",
        ToArgb(textColor), ToArgb(fillColor), ToArgb(borderColor),
        borderWidth, borderRadius, disabled);
}

void AndroidCanvasRenderer::OnDrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string&,
    const std::string&,
    const std::string&
) {
    platform::android::Bridge_CanvasDrawLink(x, y, text ? text : "", fontSize,
        fontName ? fontName : "", ToArgb(color));
}

void AndroidCanvasRenderer::OnBeginFrame() {
    platform::android::Bridge_CanvasBeginFrame(GetWidth(), GetHeight());
}

void AndroidCanvasRenderer::OnEndFrame() {
    platform::android::Bridge_CanvasEndFrame();
}

}
}
