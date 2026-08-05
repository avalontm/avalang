#include "renderer/HTMLRenderer.h"
#include "layout/FontRegistry.h"
#include "resources/ResourcePathResolver.h"
#include <iomanip>
#include <sstream>
#include <cmath>

namespace avalang {
namespace ui {

namespace {

// Canonical CSS family name used for text/buttons/links that don't
// specify a fontName (or pass "Arial"/"" from an old .avaui that never
// set one). Deliberately NOT "Arial" -- naming it after AvaUI's own
// default keeps the @font-face rule from silently colliding with a
// real "Arial" a page author might reference some other way, and makes
// it obvious in devtools that this family is AvaUI's embedded default
// rather than a system font.
constexpr const char* kDefaultCssFontFamily = "AvaDefaultFont";

// Minimal, dependency-free base64 encoder for embedding font bytes as
// a data: URI in @font-face -- see EmitFontFaceRules. Not performance
// sensitive (runs once per unique font per frame, not per glyph).
std::string Base64Encode(const unsigned char* data, std::size_t size) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= size) {
        const unsigned int n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += kTable[(n >> 6) & 0x3F];
        out += kTable[n & 0x3F];
        i += 3;
    }
    const std::size_t remaining = size - i;
    if (remaining == 1) {
        const unsigned int n = data[i] << 16;
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += "==";
    } else if (remaining == 2) {
        const unsigned int n = (data[i] << 16) | (data[i + 1] << 8);
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += kTable[(n >> 6) & 0x3F];
        out += "=";
    }
    return out;
}

// `font-family` values get sent through as attribute-adjacent CSS text
// (inside style="..."); a font name with a literal quote in it (rare,
// but project-supplied font family names are technically free-form)
// could otherwise break out of the quoted family name below.
std::string EscapeForCssString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

} // namespace

HTMLRenderer::HTMLRenderer(int width, int height)
    : BaseRenderer(width, height), outputDirty_(true) {
}

const char* HTMLRenderer::GetOutput() const {
    if (!cachedOutput_.empty()) {
        return cachedOutput_.c_str();
    }
    return "";
}

std::string HTMLRenderer::ColorToHex(const Color& c) const {
    if (c.a == 0) {
        return "transparent";
    }
    std::stringstream ss;
    ss << "#" << std::setfill('0')
       << std::setw(2) << std::hex << (int)c.r
       << std::setw(2) << std::hex << (int)c.g
       << std::setw(2) << std::hex << (int)c.b;
    return ss.str();
}

std::string HTMLRenderer::GetTransformCSS() const {
    std::stringstream ss;
    ss << "transform: ";

    if (currentTransform_.tx != 0.0f || currentTransform_.ty != 0.0f) {
        ss << "translate(" << currentTransform_.tx << "px, "
           << currentTransform_.ty << "px) ";
    }

    if (currentTransform_.sx != 1.0f || currentTransform_.sy != 1.0f) {
        ss << "scale(" << currentTransform_.sx << ", "
           << currentTransform_.sy << ") ";
    }

    if (currentTransform_.rotation != 0.0f) {
        ss << "rotate(" << (currentTransform_.rotation * 180.0f / 3.14159265f) << "deg) ";
    }

    ss << ";";
    return ss.str();
}

std::string HTMLRenderer::GetClipCSS() const {
    if (clipStack_.empty()) {
        return "";
    }

    const auto& clip = clipStack_.top();
    std::stringstream ss;
    ss << "clip-path: inset(" << clip.y << "px "
       << (width_ - clip.x - clip.w) << "px "
       << (height_ - clip.y - clip.h) << "px "
       << clip.x << "px);";
    return ss.str();
}

std::string HTMLRenderer::ResolveCssFontFamily(const char* fontName) {
    const std::string family = (fontName && fontName[0] != '\0' && std::string(fontName) != "Arial")
        ? std::string(fontName)
        : std::string(kDefaultCssFontFamily);
    usedFontNames_.insert(family);
    return family;
}

std::string HTMLRenderer::EmitFontFaceRules() const {
    // One @font-face per family name actually referenced this frame,
    // backed by the EXACT bytes FontRegistry measured that name
    // against (registered custom font, or the embedded default if the
    // name was never registered -- GetFontBytes always resolves to
    // one or the other, see FontRegistry::Resolve). This is what makes
    // the browser paint the same glyphs AvaUI's layout math already
    // assumed: the CSS family name is just a label, but the bytes
    // behind it are pinned, not resolved independently by the browser
    // the way a bare `font-family: Arial` would be.
    std::stringstream ss;
    for (const std::string& family : usedFontNames_) {
        const unsigned char* data = nullptr;
        std::size_t size = 0;
        // FontRegistry::Instance() is the same global instance
        // TextMeasure.cpp/LayoutEngineImpl used to size this text in
        // the first place -- must stay the same instance, not a fresh
        // one, or "the bytes it measured" and "the bytes we embed"
        // could drift.
        if (!layout::FontRegistry::Instance().GetFontBytes(family, &data, &size) ||
            data == nullptr || size == 0) {
            continue;
        }
        const std::string b64 = Base64Encode(data, size);
        const std::string safeFamily = EscapeForCssString(family);
        ss << "@font-face { font-family: \"" << safeFamily << "\"; "
           << "src: url(data:font/ttf;base64," << b64 << ") format(\"truetype\"); "
           << "font-weight: normal; font-style: normal; }\n";
    }
    return ss.str();
}

std::string HTMLRenderer::EmitHTMLHeader() {
    if (fragmentOnly_) return "";
    std::stringstream html_;
    html_ << "<!DOCTYPE html>\n";
    html_ << "<html lang=\"en\">\n";
    html_ << "<head>\n";
    html_ << "<meta charset=\"UTF-8\">\n";
    html_ << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html_ << "<title>" << title_ << "</title>\n";
    if (!extraHead_.empty()) html_ << extraHead_;
    html_ << "<style>\n";
    // @font-face rules first: every family name any element below
    // referenced (see ResolveCssFontFamily), each backed by the actual
    // TTF bytes FontRegistry resolved it to -- must precede the rules
    // below so `html, body { font-family: ... }`'s default resolves to
    // an already-declared face rather than a same-named system font
    // the browser found first.
    html_ << EmitFontFaceRules();
    // Fase C, opcion 2 (AVAUI_NATIVE_RENDERING_FIX_PLAN.md): the
    // viewport fills 100% of the browser window natively -- no CSS
    // transform: scale() anymore. LayoutEngine::Compute already ran
    // against width_ x height_, which is the browser's *real*
    // window.innerWidth/innerHeight rounded to kViewportResizeThresholdPx
    // (see AvaHostApp::RenderAvaUiRoute in app.cpp), so every child's
    // logical px position/size already matches the actual window --
    // .ava-viewport just needs to fill it 1:1, not scale a fixed
    // 1280x720 canvas up/down into it (the old option 1 behavior).
    // `#ava-scaler` stays as the 100vw x 100vh outer clip box;
    // `.ava-viewport` now tracks it at 100%/100% instead of a fixed px
    // size, so no transform-origin/scale is needed on it at all. A
    // resize big enough to matter is instead handled by re-requesting
    // the page with updated avaui_vw/avaui_vh cookies -- see the resize
    // listener in EmitHTMLFooter below -- so LayoutEngine actually
    // reflows at the new size server-side rather than a CSS transform
    // faking it client-side.
    html_ << "html, body { margin: 0; padding: 0; width: 100%; height: 100%; "
          << "font-family: \"" << kDefaultCssFontFamily << "\", sans-serif; "
          << "overflow: hidden; background-color: #E5E5E5; }\n";
    html_ << "#ava-scaler { position: fixed; left: 0; top: 0; width: 100vw; height: 100vh; overflow: hidden; }\n";
    html_ << ".ava-viewport { width: 100%; height: 100%; position: fixed; left: 0; top: 0; "
          << "overflow: hidden; }\n";
    html_ << ".ava-element { position: absolute; box-sizing: border-box; }\n";
    html_ << ".ava-button { margin: 0; padding: 0; display: flex; align-items: center; "
          << "justify-content: center; cursor: pointer; }\n";
    html_ << "</style>\n";
    html_ << "</head>\n";
    html_ << "<body>\n";
    html_ << "<div id=\"ava-scaler\">\n";
    html_ << "<div class=\"ava-viewport\" id=\"ava-viewport\">\n";
    return html_.str();
}

std::string HTMLRenderer::EmitHTMLFooter() {
    if (fragmentOnly_) return "";
    std::stringstream html_;
    html_ << "</div>\n";
    html_ << "</div>\n";
    // No client-side scale-to-fit and no resize-triggered reload here --
    // .ava-viewport is 100%/100% natively (see EmitHTMLHeader), and the
    // responsive-resize listener (Fase C, opcion 2) lives entirely in
    // AvaHostApp::EventScriptTag() (avahost/src/web/server/app.cpp)
    // instead of here. That's a deliberate move, not an oversight: the
    // resize flow needs to fetch the route and swap #ava-viewport's
    // content back in, which means it needs the same DOMParser/
    // scroll-restore plumbing the click/data-handler event flow already
    // has -- putting both in one place means one shared helper instead
    // of two copies, and it means the only kViewportResizeThresholdPx
    // that exists lives next to the cookies it reads/writes (app.cpp),
    // with no cross-library constant to keep in sync here in avaui.

    if (!extraBodyEnd_.empty()) html_ << extraBodyEnd_;
    html_ << "</body>\n";
    html_ << "</html>\n";
    return html_.str();
}

void HTMLRenderer::OnBeginFrame() {
    bodyHtml_.str("");
    bodyHtml_.clear();
    styleRules_.clear();
    usedFontNames_.clear();
    // Always declare the default family, even if this particular frame
    // happens to only draw elements that pass an explicit custom
    // fontName -- `html, body`'s font-family rule (see EmitHTMLHeader)
    // references it unconditionally, and an @font-face-less fallback
    // there would silently drop back to a real system sans-serif for
    // any inherited text.
    usedFontNames_.insert(kDefaultCssFontFamily);
}

void HTMLRenderer::OnEndFrame() {
    // The header can only be built now: EmitFontFaceRules (called from
    // EmitHTMLHeader) needs usedFontNames_, which OnDrawText/
    // OnDrawButton/OnDrawLink only finished populating once the body
    // above had actually been drawn. Assemble header + body + footer
    // here instead of streaming the header out first the way the
    // fixed, state-independent parts of a page normally could.
    cachedOutput_ = EmitHTMLHeader() + bodyHtml_.str() + EmitHTMLFooter();
    outputDirty_ = false;
}

void HTMLRenderer::OnDrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    const bool hasClass = !className.empty();
    // .ava-element ships position:absolute plus this function always used
    // to emit background-color/border inline regardless of hasClass.
    // Inline style beats any class by specificity, so a Tailwind class
    // like "border-b border-gray-200" was silently overridden by the
    // "border: 0px solid #000000" this function always wrote -- and the
    // element was still yanked out of flow by .ava-element's position:
    // absolute. When the user supplied their own class, give it full,
    // unfought control: skip the ava-element class and every
    // presentation default (position/size/background/border) this
    // function would otherwise inject, and rely on their CSS entirely.
    //
    // `className`/`class=` is a not-recommended escape hatch: it only
    // has meaning here, in HTMLRenderer -- GdiRenderer (desktop)
    // ignores it entirely (see GdiRenderer.cpp), so a component styled
    // via `class=` renders correctly on web but falls back to raw,
    // unstyled layout on desktop. The supported, platform-parity path
    // is native LayoutEngine properties (row/column, width/height,
    // padding, gap, align, borderWidth/borderColor/borderRadius,
    // backgroundColor), which both renderers honor identically -- see
    // docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md.
    if (hasClass) {
        bodyHtml_ << "<div class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << "<div class=\"ava-element\" style=\"";
        bodyHtml_ << "left: " << x << "px; top: " << y << "px; "
              << "width: " << width << "px; height: " << height << "px; ";
        bodyHtml_ << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; ";
        if (borderRadius > 0.0f) {
            bodyHtml_ << "border-radius: " << borderRadius << "px; ";
        }
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    bodyHtml_ << "></div>\n";
}

void HTMLRenderer::OnDrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    const bool hasClass = !className.empty();
    const float left = cx - rx;
    const float top = cy - ry;
    const float width = rx * 2.0f;
    const float height = ry * 2.0f;
    // See OnDrawRectangle: a user class gets full, unfought control --
    // no ava-element, no inline position/size/background/border. Same
    // caveat applies: `class=` only works here (HTMLRenderer); it's a
    // not-recommended escape hatch because GdiRenderer ignores it, so
    // it breaks desktop/web parity -- prefer native LayoutEngine
    // properties instead (see docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md).
    if (hasClass) {
        bodyHtml_ << "<div class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << "<div class=\"ava-element\" style=\"";
        bodyHtml_ << "left: " << left << "px; top: " << top << "px; "
              << "width: " << width << "px; height: " << height << "px; ";
        bodyHtml_ << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; "
              << "border-radius: 50%; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    bodyHtml_ << "></div>\n";
}

void HTMLRenderer::OnDrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className
) {
    const bool hasClass = !className.empty();
    // See OnDrawRectangle: a user class gets full, unfought control --
    // font-size/font-family/color inline were overriding Tailwind's
    // text-lg/font-semibold/text-color utilities the same way
    // background-color/border did for rectangles. Same caveat applies:
    // `class=` only works here (HTMLRenderer); it's a not-recommended
    // escape hatch because GdiRenderer ignores it, so it breaks
    // desktop/web parity -- prefer native LayoutEngine properties
    // (fontSize, fontName, textColor) instead (see
    // docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md).
    if (hasClass) {
        bodyHtml_ << "<div class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << "<div class=\"ava-element\" style=\"";
        bodyHtml_ << "left: " << x << "px; top: " << y << "px; ";
        bodyHtml_ << "white-space: nowrap; ";
        bodyHtml_ << "font-size: " << fontSize << "px; "
              << "font-family: \"" << ResolveCssFontFamily(fontName) << "\", sans-serif; "
              << "color: " << ColorToHex(color) << "; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    bodyHtml_ << ">" << (text ? text : "") << "</div>\n";
}

void HTMLRenderer::OnDrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    std::string resolvedSrc = resources::ResolveResourcePath(
        imagePath ? imagePath : "", resources::ResourceBackend::Web);
    bodyHtml_ << "<img class=\"ava-element\" style=\""
          << "left: " << x << "px; top: " << y << "px; "
          << "width: " << width << "px; height: " << height << "px; "
          << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\" src=\"" << resolvedSrc << "\" />\n";
}

void HTMLRenderer::OnDrawHtmlFragment(const std::string& html) {
    bodyHtml_ << html;
}

void HTMLRenderer::OnDrawButton(
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
    const bool hasClass = !className.empty();
    bodyHtml_ << "<button type=\"button\"";
    if (hasClass) {
        bodyHtml_ << " class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << " class=\"ava-element ava-button\" style=\"";
        bodyHtml_ << "left: " << x << "px; top: " << y << "px; "
              << "width: " << width << "px; height: " << height << "px; "
              << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; ";
        if (borderRadius > 0.0f) {
            bodyHtml_ << "border-radius: " << borderRadius << "px; ";
        }
        bodyHtml_ << "font-size: " << fontSize << "px; "
              << "font-family: \"" << ResolveCssFontFamily(fontName) << "\", sans-serif; "
              << "color: " << ColorToHex(textColor) << "; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (disabled) {
        bodyHtml_ << " disabled";
    }
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    bodyHtml_ << ">" << (text ? text : "") << "</button>\n";
}

void HTMLRenderer::OnDrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className
) {
    // Minimal attribute-safe escape for href -- same rationale as
    // EscapeHtmlText in SceneCommandWalker.cpp: href is normally a
    // developer-authored string from the .avaui source, not
    // end-user input, but escaping the one character that could break
    // out of the "..." attribute (a literal quote) costs nothing.
    std::string safeHref;
    safeHref.reserve(href.size());
    for (char c : href) {
        if (c == '"') {
            safeHref += "&quot;";
        } else {
            safeHref += c;
        }
    }

    const bool hasClass = !className.empty();
    bodyHtml_ << "<a href=\"" << safeHref << "\"";
    if (hasClass) {
        bodyHtml_ << " class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << " class=\"ava-element ava-link\" style=\"";
        bodyHtml_ << "left: " << x << "px; top: " << y << "px; "
              << "white-space: nowrap; text-decoration: none; "
              << "font-size: " << fontSize << "px; "
              << "font-family: \"" << ResolveCssFontFamily(fontName) << "\", sans-serif; "
              << "color: " << ColorToHex(color) << "; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    bodyHtml_ << ">" << (text ? text : "") << "</a>\n";
}

} // namespace ui
} // namespace avalang
