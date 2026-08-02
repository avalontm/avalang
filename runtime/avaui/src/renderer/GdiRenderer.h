#ifndef AVA_UI_GDI_RENDERER_H
#define AVA_UI_GDI_RENDERER_H

#include "renderer/BaseRenderer.h"
#include <windows.h>
#include <string>

namespace avalang {
namespace ui {

class GdiRenderer final : public BaseRenderer {
public:
    GdiRenderer(HWND hwnd, int width, int height);
    ~GdiRenderer() override;

    GdiRenderer(const GdiRenderer&) = delete;
    GdiRenderer& operator=(const GdiRenderer&) = delete;

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
        const std::string& className
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

    void OnBeginFrame() override;
    void OnEndFrame() override;

private:
    HWND hwnd_;
    HDC memDC_ = nullptr;
    HBITMAP memBitmap_ = nullptr;
    HBITMAP oldBitmap_ = nullptr;
    int bufferWidth_ = 0;
    int bufferHeight_ = 0;

    void EnsureBackBuffer();
    void ReleaseBackBuffer();
    static COLORREF ToColorRef(const Color& c);
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_GDI_RENDERER_H
