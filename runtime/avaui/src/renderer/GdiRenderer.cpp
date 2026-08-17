#include "renderer/GdiRenderer.h"

#include "layout/FontRegistry.h"
#include "layout/TextMeasure.h"

namespace avalang {
namespace ui {

GdiRenderer::GdiRenderer(HWND hwnd, int width, int height)
    : BaseRenderer(width, height), hwnd_(hwnd) {
}

GdiRenderer::~GdiRenderer() {
    ReleaseBackBuffer();
    // Undo AddFontMemResourceEx for every font this renderer installed
    // -- private/process-scoped installs like these are supposed to be
    // cleaned up on the way out (matches the general "match every
    // AddFontMemResourceEx with a RemoveFontMemResourceEx" GDI rule).
    for (auto& entry : loadedFontResources_) {
        if (entry.second != nullptr) {
            RemoveFontMemResourceEx(entry.second);
        }
    }
}

HFONT GdiRenderer::ResolveFont(float fontSizePx, const char* fontName, bool underline) {
    const std::string key = (fontName && fontName[0]) ? fontName : std::string();

    const auto cachedIt = resolvedFaceNames_.find(key);
    if (cachedIt != resolvedFaceNames_.end()) {
        return CreateFontA(
            -static_cast<int>(fontSizePx), 0, 0, 0, FW_NORMAL, FALSE, underline ? TRUE : FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            cachedIt->second.c_str());
    }

    const unsigned char* ttfData = nullptr;
    std::size_t ttfSize = 0;
    const bool haveBytes = layout::FontRegistry::Instance().GetFontBytes(key, &ttfData, &ttfSize);

    if (haveBytes && ttfData != nullptr && ttfSize > 0) {
        DWORD numFontsInstalled = 0;
        // AddFontMemResourceEx wants a non-const buffer; it does not
        // modify it, but the Win32 signature predates const-correctness.
        HANDLE fontHandle = AddFontMemResourceEx(
            const_cast<void*>(static_cast<const void*>(ttfData)),
            static_cast<DWORD>(ttfSize), nullptr, &numFontsInstalled);

        if (fontHandle != nullptr && numFontsInstalled > 0) {
            loadedFontResources_[key] = fontHandle;

            // The face GDI will actually respond to is whatever name
            // is baked into the TTF's own 'name' table -- for AvaUI's
            // embedded default that's "JetBrains Mono". We don't parse
            // the name table ourselves here; instead we hand GDI the
            // ORIGINAL requested key. After AddFontMemResourceEx, GDI
            // resolves that name against the newly-installed private
            // font before falling back to any system font of the same
            // name, so passing `key` (fontName) through still lands on
            // the just-installed font when they match -- and for the
            // common case (key is empty / caller didn't set fontName),
            // AvaUI's default font component property is expected to
            // literally be named after the embedded font ("JetBrains
            // Mono") by DefaultTheme, so this resolves correctly. A
            // follow-up can parse the 'name' table directly (stb
            // exposes stbtt_GetFontNameString) if a project's custom
            // font's declared family and its file's internal name ever
            // diverge.
            const std::string faceName = key.empty() ? std::string("JetBrains Mono") : key;
            resolvedFaceNames_[key] = faceName;

            return CreateFontA(
                -static_cast<int>(fontSizePx), 0, 0, 0, FW_NORMAL, FALSE, underline ? TRUE : FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                faceName.c_str());
        }
    }

    // Defensive fallback: couldn't get/install the measured font's
    // bytes (should be rare -- FontRegistry always has the built-in
    // default loaded). Resolve by name the old way rather than fail to
    // draw at all; this path means measurement and painting can drift
    // again, same as before this fix, so it's a degraded mode, not the
    // intended one.
    return CreateFontA(
        -static_cast<int>(fontSizePx), 0, 0, 0, FW_NORMAL, FALSE, underline ? TRUE : FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        (fontName && fontName[0]) ? fontName : "Segoe UI");
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
    const std::string& className,
    float maxWidth,
    bool wrap
) {
    (void)clickHandler;
    (void)className;
    if (!memDC_ || !text) return;

    HFONT font = ResolveFont(fontSize, fontName);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SetTextColor(memDC_, ToColorRef(color));

    if (wrap && maxWidth > 0.0f) {
        std::vector<std::string> lines = layout::WrapTextLines(
            std::string(text), fontSize, fontName ? fontName : std::string(), maxWidth);
        double lineHeight = layout::WrappedLineHeight(fontSize, fontName ? fontName : std::string());
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            TextOutA(memDC_, static_cast<int>(x), static_cast<int>(y + i * lineHeight),
                     line.c_str(), static_cast<int>(line.size()));
        }
    } else if (maxWidth > 0.0f) {
        RECT clip;
        clip.left = static_cast<LONG>(x);
        clip.top = static_cast<LONG>(y);
        clip.right = static_cast<LONG>(x + maxWidth);
        clip.bottom = static_cast<LONG>(y + fontSize * 1.5f);
        SIZE extent{0, 0};
        GetTextExtentPoint32A(memDC_, text, static_cast<int>(lstrlenA(text)), &extent);
        std::string out = text;
        if (extent.cx > static_cast<int>(maxWidth) && out.size() > 1) {
            const std::string ellipsis = "...";
            SIZE eExtent{0, 0};
            GetTextExtentPoint32A(memDC_, ellipsis.c_str(), 3, &eExtent);
            int target = static_cast<int>(maxWidth) - eExtent.cx;
            if (target <= 0) {
                out = ellipsis;
            } else {
                int lo = 0, hi = static_cast<int>(out.size());
                while (lo < hi) {
                    int mid = (lo + hi + 1) / 2;
                    SIZE sExtent{0, 0};
                    GetTextExtentPoint32A(memDC_, out.c_str(), mid, &sExtent);
                    if (sExtent.cx <= target) lo = mid; else hi = mid - 1;
                }
                out = out.substr(0, lo) + ellipsis;
            }
        }
        DrawTextA(memDC_, out.c_str(), static_cast<int>(out.size()), &clip,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        TextOutA(memDC_, static_cast<int>(x), static_cast<int>(y), text, static_cast<int>(lstrlenA(text)));
    }

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

    HFONT font = ResolveFont(fontSize, fontName);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SIZE extent{0, 0};
    GetTextExtentPoint32A(memDC_, text, static_cast<int>(lstrlenA(text)), &extent);
    SelectObject(memDC_, oldFont);
    DeleteObject(font);

    float offsetX = (width - static_cast<float>(extent.cx)) / 2.0f;
    float offsetY = (height - static_cast<float>(extent.cy)) / 2.0f;
    if (offsetX < 0.0f) offsetX = 0.0f;
    if (offsetY < 0.0f) offsetY = 0.0f;

    OnDrawText(x + offsetX, y + offsetY, text, fontSize, fontName, textColor, std::string(), std::string(), -1.0f, false);
}

void GdiRenderer::OnDrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className
) {
    // No real in-app navigation target on desktop (no browser/DOM to
    // hand `href` to) -- same documented "web works, desktop stub" gap
    // as IRenderer.h's SupportsScrollRegions comment. What WAS missing
    // (the actual bug) is that this used to fall all the way through
    // to OnDrawHtmlFragment, a no-op here, so a Link rendered as
    // nothing at all. Drawing the label as real, underlined text is
    // strictly better than that even without live navigation: the
    // control is visible, positioned, and its `click` handler (if any)
    // still fires through the normal hit-testing path other controls
    // use -- clickHandler/href/className aren't needed for the draw
    // itself, just kept in the signature for parity with the other
    // backends.
    (void)href;
    (void)clickHandler;
    (void)className;
    if (!memDC_ || !text) return;

    HFONT font = ResolveFont(fontSize, fontName, /*underline=*/true);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SetTextColor(memDC_, ToColorRef(color));

    TextOutA(memDC_, static_cast<int>(x), static_cast<int>(y), text, static_cast<int>(lstrlenA(text)));

    SelectObject(memDC_, oldFont);
    DeleteObject(font);
}

} // namespace ui
} // namespace avalang
