#ifndef AVA_UI_GDI_RENDERER_H
#define AVA_UI_GDI_RENDERER_H

#include "renderer/BaseRenderer.h"
#include <windows.h>
#include <string>
#include <unordered_map>

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
        float maxWidth
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

    // Loads (once, then cached) the EXACT TTF bytes
    // layout::FontRegistry measured `fontName` against -- via
    // AddFontMemResourceEx, a process-private, in-memory font
    // installation that doesn't touch the system font list -- and
    // returns an HFONT built from it at `fontSizePx`. This is what
    // makes GDI paint the same glyphs AvaUI's layout pass measured
    // instead of whatever CreateFontA happens to resolve `fontName` to
    // on this particular Windows install (see FontRegistry.h /
    // TextMeasure.h for why that mismatch was the actual bug).
    //
    // Falls back to a plain CreateFontA-by-name font only if
    // AddFontMemResourceEx itself fails (e.g. GDI resource exhaustion)
    // -- a defensive last resort, not the expected path.
    //
    // Caller owns the returned HFONT's lifetime the same as any
    // CreateFontA result (SelectObject/DeleteObject as usual); the
    // underlying *font resource* (the installed memory font) is cached
    // and released once, in the destructor, not per-HFONT.
    HFONT ResolveFont(float fontSizePx, const char* fontName, bool underline = false);

    // family name -> GDI-visible face name of the private, in-memory
    // installed font (AddFontMemResourceEx assigns the face name
    // that's baked into the TTF's own 'name' table, which is why we
    // still look it up by the *requested* family/fontName key here).
    std::unordered_map<std::string, std::string> resolvedFaceNames_;
    std::unordered_map<std::string, HANDLE> loadedFontResources_;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_GDI_RENDERER_H
