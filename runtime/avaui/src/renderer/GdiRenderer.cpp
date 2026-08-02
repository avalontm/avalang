#include "renderer/GdiRenderer.h"

namespace avalang {
namespace ui {

GdiRenderer::GdiRenderer(HWND hwnd, int width, int height)
    : BaseRenderer(width, height), hwnd_(hwnd) {
}

GdiRenderer::~GdiRenderer() {
    ReleaseBackBuffer();
}

COLORREF GdiRenderer::ToColorRef(const Color& c) {
    return RGB(c.r, c.g, c.b);
}

void GdiRenderer::ReleaseBackBuffer() {
    if (memDC_ && oldBitmap_) {
        SelectObject(memDC_, oldBitmap_);
        oldBitmap_ = nullptr;
    }
    if (memBitmap_) {
        DeleteObject(memBitmap_);
        memBitmap_ = nullptr;
    }
    if (memDC_) {
        DeleteDC(memDC_);
        memDC_ = nullptr;
    }
}

void GdiRenderer::EnsureBackBuffer() {
    if (memDC_ && bufferWidth_ == width_ && bufferHeight_ == height_) return;

    ReleaseBackBuffer();

    HDC windowDC = GetDC(hwnd_);
    memDC_ = CreateCompatibleDC(windowDC);
    memBitmap_ = CreateCompatibleBitmap(windowDC, width_ > 0 ? width_ : 1, height_ > 0 ? height_ : 1);
    oldBitmap_ = static_cast<HBITMAP>(SelectObject(memDC_, memBitmap_));
    ReleaseDC(hwnd_, windowDC);

    bufferWidth_ = width_;
    bufferHeight_ = height_;
}

void GdiRenderer::OnBeginFrame() {
    if (!hwnd_) return;

    EnsureBackBuffer();

    RECT full{0, 0, width_, height_};
    HBRUSH bg = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    FillRect(memDC_, &full, bg);

    SetBkMode(memDC_, TRANSPARENT);
}

void GdiRenderer::OnEndFrame() {
    if (!hwnd_ || !memDC_) return;

    HDC windowDC = GetDC(hwnd_);
    BitBlt(windowDC, 0, 0, width_, height_, memDC_, 0, 0, SRCCOPY);
    ReleaseDC(hwnd_, windowDC);
}

void GdiRenderer::OnDrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)clickHandler;
    // `className`/`class=` is intentionally ignored here: GdiRenderer
    // paints only the pixel coordinates the native LayoutEngine
    // already computed (row/column/padding/gap/width/height/align),
    // it has no CSS engine to resolve a class against. HTMLRenderer
    // (web) *does* apply `class=` -- see its OnDrawRectangle -- which
    // is exactly why using `class=` breaks desktop/web parity and is
    // not recommended: it silently does nothing here while changing
    // layout there. See docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md.
    (void)className;
    if (!memDC_) return;

    HBRUSH brush = CreateSolidBrush(ToColorRef(fillColor));
    HPEN pen = (borderWidth > 0.0f)
        ? CreatePen(PS_SOLID, static_cast<int>(borderWidth), ToColorRef(borderColor))
        : static_cast<HPEN>(GetStockObject(NULL_PEN));

    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));

    const int left = static_cast<int>(x);
    const int top = static_cast<int>(y);
    const int right = static_cast<int>(x + width);
    const int bottom = static_cast<int>(y + height);

    if (borderRadius > 0.0f) {
        const int diameter = static_cast<int>(borderRadius * 2.0f);
        RoundRect(memDC_, left, top, right, bottom, diameter, diameter);
    } else {
        Rectangle(memDC_, left, top, right, bottom);
    }

    SelectObject(memDC_, oldBrush);
    SelectObject(memDC_, oldPen);
    DeleteObject(brush);
    if (borderWidth > 0.0f) DeleteObject(pen);
}

void GdiRenderer::OnDrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)clickHandler;
    // See OnDrawRectangle: `class=` is a web-only, not-recommended
    // escape hatch -- ignored here on purpose.
    (void)className;
    if (!memDC_) return;

    HBRUSH brush = CreateSolidBrush(ToColorRef(fillColor));
    HPEN pen = (borderWidth > 0.0f)
        ? CreatePen(PS_SOLID, static_cast<int>(borderWidth), ToColorRef(borderColor))
        : static_cast<HPEN>(GetStockObject(NULL_PEN));

    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));

    Ellipse(memDC_,
            static_cast<int>(cx - rx), static_cast<int>(cy - ry),
            static_cast<int>(cx + rx), static_cast<int>(cy + ry));

    SelectObject(memDC_, oldBrush);
    SelectObject(memDC_, oldPen);
    DeleteObject(brush);
    if (borderWidth > 0.0f) DeleteObject(pen);
}

void GdiRenderer::OnDrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className
) {
    (void)clickHandler;
    // See OnDrawRectangle: `class=` is a web-only, not-recommended
    // escape hatch -- ignored here on purpose.
    (void)className;
    if (!memDC_ || !text) return;

    HFONT font = CreateFontA(
        -static_cast<int>(fontSize), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        (fontName && fontName[0]) ? fontName : "Segoe UI"
    );

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SetTextColor(memDC_, ToColorRef(color));

    TextOutA(memDC_, static_cast<int>(x), static_cast<int>(y), text, static_cast<int>(lstrlenA(text)));

    SelectObject(memDC_, oldFont);
    DeleteObject(font);
}

void GdiRenderer::OnDrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    if (!memDC_ || !imagePath) return;

    HBITMAP bmp = static_cast<HBITMAP>(
        LoadImageA(nullptr, imagePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE)
    );
    if (!bmp) return;

    BITMAP info{};
    GetObject(bmp, sizeof(BITMAP), &info);

    HDC srcDC = CreateCompatibleDC(memDC_);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(srcDC, bmp));

    StretchBlt(memDC_, static_cast<int>(x), static_cast<int>(y),
               static_cast<int>(width), static_cast<int>(height),
               srcDC, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);

    SelectObject(srcDC, oldBmp);
    DeleteDC(srcDC);
    DeleteObject(bmp);
}

void GdiRenderer::OnDrawHtmlFragment(const std::string& html) {
    (void)html;
}

void GdiRenderer::OnDrawButton(
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
    (void)disabled;
    if (!memDC_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius, clickHandler, className);

    if (!text || !text[0]) return;

    HFONT font = CreateFontA(
        -static_cast<int>(fontSize), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        (fontName && fontName[0]) ? fontName : "Segoe UI"
    );

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SIZE extent{0, 0};
    GetTextExtentPoint32A(memDC_, text, static_cast<int>(lstrlenA(text)), &extent);
    SelectObject(memDC_, oldFont);
    DeleteObject(font);

    float offsetX = (width - static_cast<float>(extent.cx)) / 2.0f;
    float offsetY = (height - static_cast<float>(extent.cy)) / 2.0f;
    if (offsetX < 0.0f) offsetX = 0.0f;
    if (offsetY < 0.0f) offsetY = 0.0f;

    OnDrawText(x + offsetX, y + offsetY, text, fontSize, fontName, textColor, std::string(), std::string());
}

} // namespace ui
} // namespace avalang
