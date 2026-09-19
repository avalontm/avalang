#include "platform/windows/GdiRenderer.h"

#include "layout/FontRegistry.h"
#include "layout/LayoutProperties.h"
#include "layout/TextMeasure.h"

#include <algorithm>

namespace avalang {
namespace ui {

namespace {
// El parser .avaui / el motor de bindings entregan std::string en UTF-8,
// pero TextOutA/DrawTextA/GetTextExtentPoint32A interpretan esos bytes con
// el codepage ANSI local (no UTF-8) -- cualquier caracter no-ASCII (p.ej.
// "términos") sale como mojibake ("tÃ©rminos"). Convertimos a UTF-16 y
// usamos las variantes ...W en su lugar.
std::wstring Utf8ToWide(const char* text, int lengthBytes) {
    if (!text || lengthBytes <= 0) return std::wstring();
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, lengthBytes, nullptr, 0);
    if (wideLen <= 0) return std::wstring();
    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, lengthBytes, &wide[0], wideLen);
    return wide;
}

std::wstring Utf8ToWide(const std::string& text) {
    return Utf8ToWide(text.c_str(), static_cast<int>(text.size()));
}
}  // namespace

GdiRenderer::GdiRenderer(HWND hwnd, int width, int height)
    : BaseRenderer(width, height), hwnd_(hwnd) {
}

GdiRenderer::~GdiRenderer() {
    ReleaseBackBuffer();
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
        HANDLE fontHandle = AddFontMemResourceEx(
            const_cast<void*>(static_cast<const void*>(ttfData)),
            static_cast<DWORD>(ttfSize), nullptr, &numFontsInstalled);

        if (fontHandle != nullptr && numFontsInstalled > 0) {
            loadedFontResources_[key] = fontHandle;

            const std::string faceName = key.empty() ? std::string("JetBrains Mono") : key;
            resolvedFaceNames_[key] = faceName;

            return CreateFontA(
                -static_cast<int>(fontSizePx), 0, 0, 0, FW_NORMAL, FALSE, underline ? TRUE : FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                faceName.c_str());
        }
    }

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

void GdiRenderer::OnPushClipRect(float x, float y, float width, float height) {
    if (!memDC_) return;

    SaveDC(memDC_);
    IntersectClipRect(
        memDC_,
        static_cast<int>(x), static_cast<int>(y),
        static_cast<int>(x + width), static_cast<int>(y + height));
}

void GdiRenderer::OnPopClipRect() {
    if (!memDC_) return;

    RestoreDC(memDC_, -1);
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
    (void)className;
    if (!memDC_) return;

    const int left = static_cast<int>(x);
    const int top = static_cast<int>(y);
    const int right = static_cast<int>(x + width);
    const int bottom = static_cast<int>(y + height);
    const int rectWidth = right - left;
    const int rectHeight = bottom - top;

    if (fillColor.a > 0 && fillColor.a < 255 && borderRadius <= 0.0f &&
        rectWidth > 0 && rectHeight > 0) {
        HDC srcDC = CreateCompatibleDC(memDC_);
        HBITMAP srcBitmap = CreateCompatibleBitmap(memDC_, rectWidth, rectHeight);
        HBITMAP oldSrcBitmap = static_cast<HBITMAP>(SelectObject(srcDC, srcBitmap));

        RECT fillRect{0, 0, rectWidth, rectHeight};
        HBRUSH fillBrush = CreateSolidBrush(ToColorRef(fillColor));
        FillRect(srcDC, &fillRect, fillBrush);
        DeleteObject(fillBrush);

        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = fillColor.a;
        AlphaBlend(memDC_, left, top, rectWidth, rectHeight,
                   srcDC, 0, 0, rectWidth, rectHeight, blend);

        SelectObject(srcDC, oldSrcBitmap);
        DeleteObject(srcBitmap);
        DeleteDC(srcDC);

        if (borderWidth > 0.0f) {
            HPEN pen = CreatePen(PS_SOLID, static_cast<int>(borderWidth), ToColorRef(borderColor));
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, GetStockObject(NULL_BRUSH)));
            HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));
            Rectangle(memDC_, left, top, right, bottom);
            SelectObject(memDC_, oldBrush);
            SelectObject(memDC_, oldPen);
            DeleteObject(pen);
        }
        return;
    }

    HBRUSH brush = CreateSolidBrush(ToColorRef(fillColor));
    HPEN pen = (borderWidth > 0.0f)
        ? CreatePen(PS_SOLID, static_cast<int>(borderWidth), ToColorRef(borderColor))
        : static_cast<HPEN>(GetStockObject(NULL_PEN));

    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));

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
            std::wstring wline = Utf8ToWide(lines[i]);
            TextOutW(memDC_, static_cast<int>(x), static_cast<int>(y + i * lineHeight),
                     wline.c_str(), static_cast<int>(wline.size()));
        }
    } else if (maxWidth > 0.0f) {
        RECT clip;
        clip.left = static_cast<LONG>(x);
        clip.top = static_cast<LONG>(y);
        clip.right = static_cast<LONG>(x + maxWidth);
        clip.bottom = static_cast<LONG>(y + fontSize * 1.5f);
        std::wstring wtext = Utf8ToWide(text, static_cast<int>(lstrlenA(text)));
        SIZE extent{0, 0};
        GetTextExtentPoint32W(memDC_, wtext.c_str(), static_cast<int>(wtext.size()), &extent);
        std::wstring out = wtext;
        if (extent.cx > static_cast<int>(maxWidth) && out.size() > 1) {
            const std::wstring ellipsis = L"...";
            SIZE eExtent{0, 0};
            GetTextExtentPoint32W(memDC_, ellipsis.c_str(), 3, &eExtent);
            int target = static_cast<int>(maxWidth) - eExtent.cx;
            if (target <= 0) {
                out = ellipsis;
            } else {
                int lo = 0, hi = static_cast<int>(out.size());
                while (lo < hi) {
                    int mid = (lo + hi + 1) / 2;
                    SIZE sExtent{0, 0};
                    GetTextExtentPoint32W(memDC_, out.c_str(), mid, &sExtent);
                    if (sExtent.cx <= target) lo = mid; else hi = mid - 1;
                }
                out = out.substr(0, lo) + ellipsis;
            }
        }
        DrawTextW(memDC_, out.c_str(), static_cast<int>(out.size()), &clip,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        std::wstring wtext = Utf8ToWide(text, static_cast<int>(lstrlenA(text)));
        TextOutW(memDC_, static_cast<int>(x), static_cast<int>(y), wtext.c_str(), static_cast<int>(wtext.size()));
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
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)disabled;
    if (!memDC_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius, clickHandler, className);

    if (!text || !text[0]) return;

    HFONT font = ResolveFont(fontSize, fontName);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    std::wstring wtext = Utf8ToWide(text, static_cast<int>(lstrlenA(text)));
    SIZE extent{0, 0};
    GetTextExtentPoint32W(memDC_, wtext.c_str(), static_cast<int>(wtext.size()), &extent);
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
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)href;
    (void)clickHandler;
    (void)className;
    if (!memDC_ || !text) return;

    HFONT font = ResolveFont(fontSize, fontName, true);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC_, font));
    SetTextColor(memDC_, ToColorRef(color));

    std::wstring wtext = Utf8ToWide(text, static_cast<int>(lstrlenA(text)));
    TextOutW(memDC_, static_cast<int>(x), static_cast<int>(y), wtext.c_str(), static_cast<int>(wtext.size()));

    SelectObject(memDC_, oldFont);
    DeleteObject(font);
}

void GdiRenderer::OnDrawInput(
    float x, float y, float width, float height,
    const std::string& text,
    const std::string& placeholder,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled, bool focused, bool hovered,
    int caretIndex, int selectionStart, int selectionEnd,
    const std::string& imeComposition, int imeCompositionCursor,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)hovered;
    (void)clickHandler;
    (void)className;
    if (!memDC_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius,
                     std::string(), std::string());

    const bool composing = focused && !imeComposition.empty();
    std::string content = text;
    int compositionStart = -1;
    int compositionEnd = -1;
    int displayCaret = caretIndex;

    if (composing) {
        const int insertAt = (caretIndex >= 0 && caretIndex <= static_cast<int>(text.size()))
                                  ? caretIndex : static_cast<int>(text.size());
        content = text.substr(0, insertAt) + imeComposition + text.substr(insertAt);
        compositionStart = insertAt;
        compositionEnd = insertAt + static_cast<int>(imeComposition.size());
        displayCaret = insertAt + std::min(imeCompositionCursor, static_cast<int>(imeComposition.size()));
    }

    const bool showingPlaceholder = content.empty() && !placeholder.empty();
    const std::string& shown = showingPlaceholder ? placeholder : content;

    const float paddingX = static_cast<float>(layout::kDefaultInputPaddingX);
    const float textX = x + paddingX;
    const float textY = y + (height - fontSize * 1.2f) / 2.0f;
    const float maxWidth = width - 2.0f * paddingX;

    Color displayColor = textColor;
    if (showingPlaceholder) {
        displayColor.r = static_cast<std::uint8_t>((textColor.r + 255) / 2);
        displayColor.g = static_cast<std::uint8_t>((textColor.g + 255) / 2);
        displayColor.b = static_cast<std::uint8_t>((textColor.b + 255) / 2);
    }

    float caretX = textX;
    float selX0 = textX;
    float selX1 = textX;
    float compX0 = textX;
    float compX1 = textX;
    const bool hasSelection = !showingPlaceholder && !composing &&
                               selectionStart >= 0 && selectionEnd > selectionStart;
    const bool hasComposition = !showingPlaceholder && composing && compositionEnd > compositionStart;

    if (!showingPlaceholder) {
        HFONT measureFont = ResolveFont(fontSize, fontName);
        HFONT oldMeasureFont = static_cast<HFONT>(SelectObject(memDC_, measureFont));

        // NOTA: sigue midiendo con la variante ANSI sobre bytes UTF-8 --
        // el caret puede quedar mal posicionado si el texto antes del
        // cursor tiene caracteres no-ASCII. No es el bug reportado (texto
        // visible / interactividad); si hace falta, aplicar el mismo
        // patron Utf8ToWide + GetTextExtentPoint32W que arriba, con offsets
        // en unidades UTF-16 en vez de bytes.
        auto widthUpTo = [&](int byteOffset) -> float {
            const int clamped = std::max(0, std::min(byteOffset, static_cast<int>(shown.size())));
            if (clamped == 0) return 0.0f;
            SIZE extent{0, 0};
            GetTextExtentPoint32A(memDC_, shown.c_str(), clamped, &extent);
            return static_cast<float>(extent.cx);
        };

        caretX = textX + widthUpTo(displayCaret >= 0 ? displayCaret : static_cast<int>(shown.size()));
        if (hasSelection) {
            selX0 = textX + widthUpTo(selectionStart);
            selX1 = textX + widthUpTo(selectionEnd);
        }
        if (hasComposition) {
            compX0 = textX + widthUpTo(compositionStart);
            compX1 = textX + widthUpTo(compositionEnd);
        }

        SelectObject(memDC_, oldMeasureFont);
        DeleteObject(measureFont);
    }

    if (hasSelection) {
        HBRUSH selectionBrush = CreateSolidBrush(RGB(173, 214, 255));
        RECT selectionRect{static_cast<LONG>(selX0), static_cast<LONG>(y + 3.0f),
                           static_cast<LONG>(selX1), static_cast<LONG>(y + height - 3.0f)};
        FillRect(memDC_, &selectionRect, selectionBrush);
        DeleteObject(selectionBrush);
    }

    if (!shown.empty()) {
        OnDrawText(textX, textY, shown.c_str(), fontSize, fontName, displayColor,
                   std::string(), std::string(), maxWidth, false);
    }

    if (hasComposition) {
        HPEN underlinePen = CreatePen(PS_SOLID, 1, ToColorRef(textColor));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, underlinePen));
        const int underlineY = static_cast<int>(textY + fontSize * 1.1f);
        MoveToEx(memDC_, static_cast<int>(compX0), underlineY, nullptr);
        LineTo(memDC_, static_cast<int>(compX1), underlineY);
        SelectObject(memDC_, oldPen);
        DeleteObject(underlinePen);
    }

    const bool blinkOn = (GetTickCount() / 500) % 2 == 0;
    if (focused && !disabled && blinkOn) {
        HPEN caretPen = CreatePen(PS_SOLID, 1, ToColorRef(textColor));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, caretPen));
        MoveToEx(memDC_, static_cast<int>(caretX) + 1, static_cast<int>(y + 4.0f), nullptr);
        LineTo(memDC_, static_cast<int>(caretX) + 1, static_cast<int>(y + height - 4.0f));
        SelectObject(memDC_, oldPen);
        DeleteObject(caretPen);
    }
}

void GdiRenderer::OnDrawCheckBox(
    float x, float y, float width, float height,
    const std::string& text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& boxFillColor,
    const Color& boxBorderColor, float borderWidth, float borderRadius,
    bool checked, bool disabled, bool focused, bool hovered,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)hovered;
    (void)clickHandler;
    (void)className;
    if (!memDC_) return;

    const float defaultBoxSize = static_cast<float>(layout::kDefaultCheckboxBoxSize);
    const float boxSize = std::min(defaultBoxSize, height > 0.0f ? height : defaultBoxSize);
    const float boxY = y + (height - boxSize) / 2.0f;

    OnDrawRectangle(x, boxY, boxSize, boxSize, boxFillColor, boxBorderColor, borderWidth, borderRadius,
                     std::string(), std::string());

    if (checked) {
        HPEN pen = CreatePen(PS_SOLID, 2, ToColorRef(textColor));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));
        const int left = static_cast<int>(x);
        const int top = static_cast<int>(boxY);
        MoveToEx(memDC_, left + static_cast<int>(boxSize * 0.2f), top + static_cast<int>(boxSize * 0.55f), nullptr);
        LineTo(memDC_, left + static_cast<int>(boxSize * 0.45f), top + static_cast<int>(boxSize * 0.8f));
        LineTo(memDC_, left + static_cast<int>(boxSize * 0.8f), top + static_cast<int>(boxSize * 0.2f));
        SelectObject(memDC_, oldPen);
        DeleteObject(pen);
    }

    if (focused && !disabled) {
        HPEN pen = CreatePen(PS_SOLID, 1, ToColorRef(boxBorderColor));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, GetStockObject(NULL_BRUSH)));
        Rectangle(memDC_, static_cast<int>(x - 2.0f), static_cast<int>(boxY - 2.0f),
                  static_cast<int>(x + boxSize + 2.0f), static_cast<int>(boxY + boxSize + 2.0f));
        SelectObject(memDC_, oldBrush);
        SelectObject(memDC_, oldPen);
        DeleteObject(pen);
    }

    if (!text.empty()) {
        const float labelX = x + boxSize + static_cast<float>(layout::kDefaultCheckboxLabelGap);
        const float labelY = y + (height - fontSize * 1.2f) / 2.0f;
        OnDrawText(labelX, labelY, text.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), -1.0f, false);
    }
}

void GdiRenderer::OnDrawRadioButton(
    float x, float y, float width, float height,
    const std::string& text,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& boxFillColor,
    const Color& boxBorderColor, float borderWidth,
    bool selected, bool disabled, bool focused, bool hovered,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)hovered;
    (void)clickHandler;
    (void)className;
    if (!memDC_) return;

    const float defaultBoxSize = static_cast<float>(layout::kDefaultCheckboxBoxSize);
    const float boxSize = std::min(defaultBoxSize, height > 0.0f ? height : defaultBoxSize);
    const float boxY = y + (height - boxSize) / 2.0f;
    const float cx = x + boxSize / 2.0f;
    const float cy = boxY + boxSize / 2.0f;

    OnDrawEllipse(cx, cy, boxSize / 2.0f, boxSize / 2.0f, boxFillColor, boxBorderColor, borderWidth,
                  std::string(), std::string());

    if (selected) {
        HBRUSH brush = CreateSolidBrush(ToColorRef(textColor));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, brush));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, GetStockObject(NULL_PEN)));
        const float dotRadius = boxSize * 0.22f;
        Ellipse(memDC_, static_cast<int>(cx - dotRadius), static_cast<int>(cy - dotRadius),
                static_cast<int>(cx + dotRadius), static_cast<int>(cy + dotRadius));
        SelectObject(memDC_, oldPen);
        SelectObject(memDC_, oldBrush);
        DeleteObject(brush);
    }

    if (focused && !disabled) {
        HPEN pen = CreatePen(PS_SOLID, 1, ToColorRef(boxBorderColor));
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, GetStockObject(NULL_BRUSH)));
        Ellipse(memDC_, static_cast<int>(x - 2.0f), static_cast<int>(boxY - 2.0f),
                static_cast<int>(x + boxSize + 2.0f), static_cast<int>(boxY + boxSize + 2.0f));
        SelectObject(memDC_, oldBrush);
        SelectObject(memDC_, oldPen);
        DeleteObject(pen);
    }

    if (!text.empty()) {
        const float labelX = x + boxSize + static_cast<float>(layout::kDefaultCheckboxLabelGap);
        const float labelY = y + (height - fontSize * 1.2f) / 2.0f;
        OnDrawText(labelX, labelY, text.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), -1.0f, false);
    }
}

void GdiRenderer::OnDrawComboBox(
    float x, float y, float width, float height,
    const std::vector<render::ComboBoxItem>& items,
    float fontSize, const char* fontName,
    const Color& textColor,
    const Color& fillColor,
    const Color& borderColor, float borderWidth, float borderRadius,
    bool disabled, bool focused, bool hovered, bool open,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId,
    const std::string&
) {
    (void)disabled;
    (void)hovered;
    (void)clickHandler;
    (void)className;
    if (!memDC_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius,
                     std::string(), std::string());

    std::string selectedLabel;
    for (const auto& item : items) {
        if (item.selected) {
            selectedLabel = item.label;
            break;
        }
    }

    const float paddingX = static_cast<float>(layout::kDefaultInputPaddingX);
    const float arrowSize = 8.0f;
    const float textX = x + paddingX;
    const float textY = y + (height - fontSize * 1.2f) / 2.0f;
    const float maxWidth = width - 2.0f * paddingX - arrowSize;

    if (!selectedLabel.empty()) {
        OnDrawText(textX, textY, selectedLabel.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), maxWidth, false);
    }

    const float arrowCx = x + width - paddingX - arrowSize / 2.0f;
    const float arrowCy = y + height / 2.0f;
    HPEN arrowPen = CreatePen(PS_SOLID, 1, ToColorRef(textColor));
    HPEN oldArrowPen = static_cast<HPEN>(SelectObject(memDC_, arrowPen));
    MoveToEx(memDC_, static_cast<int>(arrowCx - arrowSize / 2.0f), static_cast<int>(arrowCy - arrowSize / 4.0f), nullptr);
    LineTo(memDC_, static_cast<int>(arrowCx), static_cast<int>(arrowCy + arrowSize / 4.0f));
    LineTo(memDC_, static_cast<int>(arrowCx + arrowSize / 2.0f), static_cast<int>(arrowCy - arrowSize / 4.0f));
    SelectObject(memDC_, oldArrowPen);
    DeleteObject(arrowPen);

    if (focused) {
        HPEN focusPen = CreatePen(PS_SOLID, 1, ToColorRef(borderColor));
        HPEN oldFocusPen = static_cast<HPEN>(SelectObject(memDC_, focusPen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC_, GetStockObject(NULL_BRUSH)));
        Rectangle(memDC_, static_cast<int>(x - 1.0f), static_cast<int>(y - 1.0f),
                  static_cast<int>(x + width + 1.0f), static_cast<int>(y + height + 1.0f));
        SelectObject(memDC_, oldBrush);
        SelectObject(memDC_, oldFocusPen);
        DeleteObject(focusPen);
    }

    if (!open || items.empty()) return;

    float rowY = y + height;
    for (const auto& item : items) {
        const Color rowFill = item.selected ? borderColor : fillColor;
        OnDrawRectangle(x, rowY, width, height, rowFill, borderColor, 1.0f, 0.0f,
                         std::string(), std::string());
        OnDrawText(x + paddingX, rowY + (height - fontSize * 1.2f) / 2.0f,
                   item.label.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), width - 2.0f * paddingX, false);
        rowY += height;
    }
}

}
}