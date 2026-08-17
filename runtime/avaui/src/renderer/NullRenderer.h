#ifndef AVA_UI_NULL_RENDERER_H
#define AVA_UI_NULL_RENDERER_H

#include "renderer/BaseRenderer.h"
#include <string>

namespace avalang {
namespace ui {

class NullRenderer final : public BaseRenderer {
public:
    NullRenderer(int width, int height);
    ~NullRenderer() override = default;

    NullRenderer(const NullRenderer&) = delete;
    NullRenderer& operator=(const NullRenderer&) = delete;

protected:
    void OnDrawRectangle(
        float x, float y, float width, float height,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        float borderRadius,
        const std::string& clickHandler,
        const std::string& className
    ) override;

    void OnDrawEllipse(
        float cx, float cy, float rx, float ry,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        const std::string& clickHandler,
        const std::string& className
    ) override;

    void OnDrawText(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& clickHandler,
        const std::string& className,
        float maxWidth,
        bool wrap
    ) override;

    void OnDrawImage(
        float x, float y, float width, float height,
        const char* imagePath
    ) override;

    void OnDrawHtmlFragment(const std::string& html) override;

    void OnDrawButton(
        float x, float y, float width, float height,
        const char* text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled,
        const std::string& clickHandler,
        const std::string& className
    ) override;

    void OnDrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler,
        const std::string& className
    ) override;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_NULL_RENDERER_H
