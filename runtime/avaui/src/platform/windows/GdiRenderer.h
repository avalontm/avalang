#pragma once

#include "renderer/BaseRenderer.h"

#include <string>
#include <unordered_map>
#include <windows.h>

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

    HFONT ResolveFont(float fontSizePx, const char* fontName, bool underline = false);

    std::unordered_map<std::string, std::string> resolvedFaceNames_;
    std::unordered_map<std::string, HANDLE> loadedFontResources_;
};

}
}