#include "renderer/HTMLRenderer.h"
#include "resources/ResourcePathResolver.h"
#include <iomanip>
#include <sstream>
#include <cmath>

namespace avalang {
namespace ui {

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

void HTMLRenderer::EmitHTMLHeader() {
    if (fragmentOnly_) return;
    html_ << "<!DOCTYPE html>\n";
    html_ << "<html lang=\"en\">\n";
    html_ << "<head>\n";
    html_ << "<meta charset=\"UTF-8\">\n";
    html_ << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html_ << "<title>" << title_ << "</title>\n";
    if (!extraHead_.empty()) html_ << extraHead_;
    html_ << "<style>\n";
    // Fase C, option 2 (AVAUI_NATIVE_RENDERING_FIX_PLAN.md): the
    // viewport fills 100% of the browser window by default instead of
    // sitting as a fixed width_ x height_ box centered on a gray
    // backdrop (option 1, the previous behavior). LayoutEngine still
    // computes every child's position/size in logical px against
    // width_ x height_ (unchanged -- no layout recompute per resize),
    // but here that logical canvas is rendered at its native size and
    // then CSS-scaled (independently per axis, via `transform: scale`)
    // to exactly fill whatever window it's shown in. `#ava-scaler` is
    // the 100vw x 100vh window-filling box; `.ava-viewport` keeps its
    // logical width_ x height_ size and gets scaled/positioned inside
    // it by the inline script below. This only changes host chrome
    // (this file), not app content or the layout engine.
    html_ << "html, body { margin: 0; padding: 0; width: 100%; height: 100%; "
          << "font-family: Arial, sans-serif; overflow: hidden; background-color: #E5E5E5; }\n";
    html_ << "#ava-scaler { position: fixed; left: 0; top: 0; width: 100vw; height: 100vh; overflow: hidden; }\n";
    html_ << ".ava-viewport { width: " << width_ << "px; height: " << height_
          << "px; position: fixed; left: 0; top: 0; transform-origin: top left; "
          << "overflow: hidden; }\n";
    html_ << ".ava-element { position: absolute; box-sizing: border-box; }\n";
    html_ << ".ava-button { margin: 0; padding: 0; display: flex; align-items: center; "
          << "justify-content: center; cursor: pointer; }\n";
    html_ << "</style>\n";
    html_ << "</head>\n";
    html_ << "<body>\n";
    html_ << "<div id=\"ava-scaler\">\n";
    html_ << "<div class=\"ava-viewport\" id=\"ava-viewport\">\n";
}

void HTMLRenderer::EmitHTMLFooter() {
    if (fragmentOnly_) return;
    html_ << "</div>\n";
    html_ << "</div>\n";
    // Scales the logical width_ x height_ canvas uniformly (one scale
    // factor, not independent per axis) to fit inside the window and
    // centers it -- preserves the design's proportions so text/buttons
    // don't stretch or squish when the window's aspect ratio doesn't
    // match the canvas's. Trade-off: a window with a very different
    // aspect ratio (e.g. narrow/tall) gets empty margin on one axis
    // instead of filling both completely -- true full-bleed on every
    // window shape would need the layout engine itself to recompute
    // against the new aspect ratio, not just a CSS transform. Runs on
    // load and on resize; no server round-trip, no layout recompute.
    html_ << "<script>\n";
    html_ << "(function(){\n";
    html_ << "  var vp = document.getElementById('ava-viewport');\n";
    html_ << "  var baseW = " << width_ << ", baseH = " << height_ << ";\n";
    html_ << "  function fit(){\n";
    html_ << "    var s = Math.min(window.innerWidth / baseW, window.innerHeight / baseH);\n";
    html_ << "    var offsetX = (window.innerWidth - baseW * s) / 2;\n";
    html_ << "    var offsetY = (window.innerHeight - baseH * s) / 2;\n";
    html_ << "    vp.style.transform = 'scale(' + s + ')';\n";
    html_ << "    vp.style.left = offsetX + 'px';\n";
    html_ << "    vp.style.top = offsetY + 'px';\n";
    html_ << "  }\n";
    html_ << "  fit();\n";
    html_ << "  window.addEventListener('resize', fit);\n";
    html_ << "})();\n";
    html_ << "</script>\n";
    if (!extraBodyEnd_.empty()) html_ << extraBodyEnd_;
    html_ << "</body>\n";
    html_ << "</html>\n";
}

void HTMLRenderer::OnBeginFrame() {
    html_.str("");
    html_.clear();
    styleRules_.clear();
    EmitHTMLHeader();
}

void HTMLRenderer::OnEndFrame() {
    EmitHTMLFooter();
    cachedOutput_ = html_.str();
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
        html_ << "<div class=\"" << className << "\" style=\"";
    } else {
        html_ << "<div class=\"ava-element\" style=\"";
        html_ << "left: " << x << "px; top: " << y << "px; "
              << "width: " << width << "px; height: " << height << "px; ";
        html_ << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; ";
        if (borderRadius > 0.0f) {
            html_ << "border-radius: " << borderRadius << "px; ";
        }
    }
    html_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        html_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    html_ << "></div>\n";
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
        html_ << "<div class=\"" << className << "\" style=\"";
    } else {
        html_ << "<div class=\"ava-element\" style=\"";
        html_ << "left: " << left << "px; top: " << top << "px; "
              << "width: " << width << "px; height: " << height << "px; ";
        html_ << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; "
              << "border-radius: 50%; ";
    }
    html_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        html_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    html_ << "></div>\n";
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
        html_ << "<div class=\"" << className << "\" style=\"";
    } else {
        html_ << "<div class=\"ava-element\" style=\"";
        html_ << "left: " << x << "px; top: " << y << "px; ";
        html_ << "white-space: nowrap; ";
        html_ << "font-size: " << fontSize << "px; "
              << "font-family: " << (fontName ? fontName : "Arial") << "; "
              << "color: " << ColorToHex(color) << "; ";
    }
    html_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        html_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    html_ << ">" << (text ? text : "") << "</div>\n";
}

void HTMLRenderer::OnDrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    std::string resolvedSrc = resources::ResolveResourcePath(
        imagePath ? imagePath : "", resources::ResourceBackend::Web);
    html_ << "<img class=\"ava-element\" style=\""
          << "left: " << x << "px; top: " << y << "px; "
          << "width: " << width << "px; height: " << height << "px; "
          << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\" src=\"" << resolvedSrc << "\" />\n";
}

void HTMLRenderer::OnDrawHtmlFragment(const std::string& html) {
    html_ << html;
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
    html_ << "<button type=\"button\"";
    if (hasClass) {
        html_ << " class=\"" << className << "\" style=\"";
    } else {
        html_ << " class=\"ava-element ava-button\" style=\"";
        html_ << "left: " << x << "px; top: " << y << "px; "
              << "width: " << width << "px; height: " << height << "px; "
              << "background-color: " << ColorToHex(fillColor) << "; "
              << "border: " << borderWidth << "px solid " << ColorToHex(borderColor) << "; ";
        if (borderRadius > 0.0f) {
            html_ << "border-radius: " << borderRadius << "px; ";
        }
        html_ << "font-size: " << fontSize << "px; "
              << "font-family: " << (fontName ? fontName : "Arial") << "; "
              << "color: " << ColorToHex(textColor) << "; ";
    }
    html_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (disabled) {
        html_ << " disabled";
    }
    if (!clickHandler.empty()) {
        html_ << " data-event=\"click\" data-handler=\"" << clickHandler << "\"";
    }
    html_ << ">" << (text ? text : "") << "</button>\n";
}

} // namespace ui
} // namespace avalang