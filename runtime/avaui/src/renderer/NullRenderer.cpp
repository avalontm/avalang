#include "renderer/NullRenderer.h"
#include <string>

namespace avalang {
namespace ui {

NullRenderer::NullRenderer(int width, int height)
    : BaseRenderer(width, height) {
}

void NullRenderer::OnDrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)x; (void)y; (void)width; (void)height;
    (void)fillColor; (void)borderColor; (void)borderWidth;
    (void)borderRadius; (void)clickHandler; (void)className;
}

void NullRenderer::OnDrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)cx; (void)cy; (void)rx; (void)ry;
    (void)fillColor; (void)borderColor; (void)borderWidth;
    (void)clickHandler; (void)className;
}

void NullRenderer::OnDrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className,
    float maxWidth,
    bool wrap
) {
    (void)x; (void)y; (void)text; (void)fontSize;
    (void)fontName; (void)color; (void)clickHandler; (void)className; (void)maxWidth; (void)wrap;
}

void NullRenderer::OnDrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    (void)x; (void)y; (void)width; (void)height; (void)imagePath;
}

void NullRenderer::OnDrawHtmlFragment(const std::string& html) {
    (void)html;
}

void NullRenderer::OnDrawButton(
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
    (void)x; (void)y; (void)width; (void)height; (void)text;
    (void)fontSize; (void)fontName; (void)textColor; (void)fillColor;
    (void)borderColor; (void)borderWidth; (void)borderRadius; (void)disabled;
    (void)clickHandler; (void)className;
}

void NullRenderer::OnDrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)x; (void)y; (void)text; (void)fontSize; (void)fontName;
    (void)color; (void)href; (void)clickHandler; (void)className;
}

} // namespace ui
} // namespace avalang
