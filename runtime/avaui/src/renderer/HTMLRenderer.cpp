#include "renderer/HTMLRenderer.h"
#include "layout/FontRegistry.h"
#include "resources/ResourcePathResolver.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>

namespace avalang {
namespace ui {

namespace {

constexpr const char* kDefaultCssFontFamily = "AvaDefaultFont";

std::string EscapeHtmlAttr(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

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

std::string SanitizeFontFileName(const std::string& family) {
    std::string out;
    out.reserve(family.size());
    for (char c : family) {
        const bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_';
        out += safe ? c : '_';
    }
    if (out.empty()) out = "font";
    return out + ".ttf";
}

struct DialogAnimationValues {
    double from;
    double to;
    std::string duration;
    std::string easing;
};

DialogAnimationValues ResolveDialogAnimation(const theme::ProjectAnimationSheet* animations,
                                              const std::string& trigger,
                                              double defaultFrom, double defaultTo,
                                              const std::string& defaultDuration,
                                              const std::string& defaultEasing) {
    DialogAnimationValues result{defaultFrom, defaultTo, defaultDuration, defaultEasing};
    if (!animations) return result;
    const theme::AnimationOverride o = animations->Resolve("dialog", trigger);
    if (o.from) result.from = *o.from;
    if (o.to) result.to = *o.to;
    if (o.duration) result.duration = *o.duration;
    if (o.easing) result.easing = *o.easing;
    return result;
}

const char* CssClassForControlType(const std::string& typeLower) {
    if (typeLower == "button") return "ava-button";
    if (typeLower == "textbox") return "ava-input";
    if (typeLower == "combobox") return "ava-select";
    if (typeLower == "link") return "ava-link";
    if (typeLower == "text") return "ava-text";
    return nullptr;
}

void AppendStateDeclarations(std::stringstream& css, const theme::ControlStyleOverride& o) {
    if (o.backgroundColor) css << "background-color: #" << *o.backgroundColor << " !important; ";
    if (o.textColor) css << "color: #" << *o.textColor << " !important; ";
    if (o.borderColor) css << "border-color: #" << *o.borderColor << " !important; ";
    if (o.borderWidth) css << "border-width: " << *o.borderWidth << "px !important; ";
    if (o.borderRadius) css << "border-radius: " << *o.borderRadius << "px !important; ";
    if (o.fontSize) css << "font-size: " << *o.fontSize << "px !important; ";
}

}

HTMLRenderer::HTMLRenderer(int width, int height)
    : BaseRenderer(width, height) {
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
    std::string sanitized;
    sanitized.reserve(family.size());
    for (char c : family) {
        if (c != '\'' && c != '"') sanitized += c;
    }
    usedFontNames_.insert(sanitized);
    return sanitized;
}

std::string HTMLRenderer::EmitFontFaceRules() const {
    std::set<std::string> allFamilies = usedFontNames_;
    for (const std::string& registered : layout::FontRegistry::Instance().RegisteredFontNames()) {
        allFamilies.insert(registered);
    }

    std::stringstream ss;
    for (const std::string& family : allFamilies) {
        const unsigned char* data = nullptr;
        std::size_t size = 0;
        if (!layout::FontRegistry::Instance().GetFontBytes(family, &data, &size) ||
            data == nullptr || size == 0) {
            continue;
        }
        const std::string safeFamily = EscapeForCssString(family);

        std::string fontUrl;
        if (!wwwRootDir_.empty()) {
            const std::string fileName = SanitizeFontFileName(family);
            std::filesystem::path fontsDir = std::filesystem::path(wwwRootDir_) / "fonts";
            std::filesystem::path fontFile = fontsDir / fileName;
            std::error_code ec;
            if (!std::filesystem::exists(fontFile, ec)) {
                std::filesystem::create_directories(fontsDir, ec);
                std::ofstream out(fontFile, std::ios::binary | std::ios::trunc);
                if (out) {
                    out.write(reinterpret_cast<const char*>(data),
                              static_cast<std::streamsize>(size));
                }
            }
            if (!ec && std::filesystem::exists(fontFile, ec)) {
                fontUrl = "/fonts/" + fileName;
            }
        }

        if (!fontUrl.empty()) {
            ss << "@font-face { font-family: \"" << safeFamily << "\"; "
               << "src: url(\"" << fontUrl << "\") format(\"truetype\"); "
               << "font-weight: normal; font-style: normal; }\n";
        } else {
            const std::string b64 = Base64Encode(data, size);
            ss << "@font-face { font-family: \"" << safeFamily << "\"; "
               << "src: url(data:font/ttf;base64," << b64 << ") format(\"truetype\"); "
               << "font-weight: normal; font-style: normal; }\n";
        }
    }
    return ss.str();
}

std::string HTMLRenderer::EmitStaticBaseCssLink() const {
    static const std::string kBaseCss =
        "html, body { margin: 0; padding: 0; width: 100%; height: 100%; "
        "font-family: \"" + std::string(kDefaultCssFontFamily) + "\", sans-serif; "
        "overflow: hidden; background-color: #E5E5E5; }\n"
        "#ava-scaler { position: fixed; left: 0; top: 0; width: 100vw; height: 100vh; overflow: hidden; }\n"
        ".ava-viewport { width: 100%; height: 100%; position: fixed; left: 0; top: 0; "
        "overflow: hidden; }\n"
        ".ava-element { position: absolute; box-sizing: border-box; overflow: hidden; }\n"
        ".ava-button { margin: 0; padding: 0; display: flex; align-items: center; "
        "justify-content: center; cursor: pointer; "
        "-webkit-appearance: none; -moz-appearance: none; appearance: none; }\n";

    if (!wwwRootDir_.empty()) {
        std::filesystem::path cssDir = std::filesystem::path(wwwRootDir_) / "css";
        std::filesystem::path cssFile = cssDir / "ava-runtime.css";
        std::error_code ec;
        if (!std::filesystem::exists(cssFile, ec)) {
            std::filesystem::create_directories(cssDir, ec);
            std::ofstream out(cssFile, std::ios::binary | std::ios::trunc);
            if (out) {
                out.write(kBaseCss.data(), static_cast<std::streamsize>(kBaseCss.size()));
            }
        }
        if (!ec && std::filesystem::exists(cssFile, ec)) {
            return "<link rel=\"stylesheet\" href=\"/css/ava-runtime.css\">\n";
        }
    }
    return "<style>\n" + kBaseCss + "</style>\n";
}

std::string HTMLRenderer::EmitProjectCssLink(const std::string& dynamicCss) const {
    if (!wwwRootDir_.empty()) {
        std::filesystem::path cssDir = std::filesystem::path(wwwRootDir_) / "css";
        std::filesystem::path cssFile = cssDir / "ava-project.css";
        std::error_code ec;
        std::filesystem::create_directories(cssDir, ec);
        bool wrote = false;
        if (!ec) {
            std::ofstream out(cssFile, std::ios::binary | std::ios::trunc);
            if (out) {
                out.write(dynamicCss.data(), static_cast<std::streamsize>(dynamicCss.size()));
                wrote = static_cast<bool>(out);
            }
        }
        if (wrote) {
            const std::string tag = std::to_string(std::hash<std::string>{}(dynamicCss));
            return "<link rel=\"stylesheet\" href=\"/css/ava-project.css?v=" + tag + "\">\n";
        }
    }
    return "<style>\n" + dynamicCss + "</style>\n";
}

std::string HTMLRenderer::EmitProjectStateCSS() const {
    if (!projectStyles_ || !projectStyles_->HasAnyStateStyles()) return "";

    static const char* kTypes[] = {"button", "textbox", "combobox", "link", "text"};
    static const char* kStates[] = {"hover", "focus", "active", "disabled"};

    std::stringstream css;
    for (const char* type : kTypes) {
        const char* cssClass = CssClassForControlType(type);
        if (!cssClass) continue;
        for (const char* state : kStates) {
            const theme::ControlStyleOverride o = projectStyles_->ResolveState(type, state);
            std::stringstream decls;
            AppendStateDeclarations(decls, o);
            const std::string declStr = decls.str();
            if (declStr.empty()) continue;
            const std::string pseudo = (std::string(state) == "disabled")
                ? std::string(":disabled")
                : (":" + std::string(state));
            css << "." << cssClass << pseudo << " { " << declStr << "}\n";
        }
    }
    return css.str();
}

std::string HTMLRenderer::EmitProjectResponsiveCSS() const {
    if (!projectStyles_ || !projectStyles_->HasAnyResponsiveStyles()) return "";

    static const char* kTypes[] = {"button", "textbox", "combobox", "link", "text"};

    std::stringstream css;
    for (const theme::BreakpointOverride& breakpoint : projectStyles_->Breakpoints()) {
        std::stringstream body;
        if (breakpoint.hasGlobal) {
            std::stringstream decls;
            AppendStateDeclarations(decls, breakpoint.global);
            const std::string declStr = decls.str();
            if (!declStr.empty()) {
                body << ".ava-element { " << declStr << "}\n";
            }
        }
        for (const char* type : kTypes) {
            const char* cssClass = CssClassForControlType(type);
            if (!cssClass) continue;
            const auto it = breakpoint.perType.find(type);
            if (it == breakpoint.perType.end()) continue;
            std::stringstream decls;
            AppendStateDeclarations(decls, it->second);
            const std::string declStr = decls.str();
            if (declStr.empty()) continue;
            body << "." << cssClass << " { " << declStr << "}\n";
        }
        const std::string bodyStr = body.str();
        if (bodyStr.empty()) continue;
        css << "@media (min-width: " << breakpoint.minWidthPx << "px) {\n" << bodyStr << "}\n";
    }
    return css.str();
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
    html_ << EmitStaticBaseCssLink();
    std::stringstream dynamicCss;
    dynamicCss << EmitFontFaceRules();
    const DialogAnimationValues openAnim = ResolveDialogAnimation(
        projectAnimations_, "open", 0.0, 1.0,
        "160ms", "ease-out");
    const DialogAnimationValues closeAnim = ResolveDialogAnimation(
        projectAnimations_, "close", 1.0, 0.0,
        "160ms", "ease-in");
    dynamicCss << "@keyframes ava-overlay-fade-in { from { opacity: " << openAnim.from
               << "; } to { opacity: " << openAnim.to << "; } }\n";
    dynamicCss << "@keyframes ava-overlay-fade-out { from { opacity: " << closeAnim.from
               << "; } to { opacity: " << closeAnim.to << "; } }\n";
    dynamicCss << ".ava-overlay-fragment { animation: ava-overlay-fade-in " << openAnim.duration
               << " " << openAnim.easing << "; }\n";
    dynamicCss << ".ava-overlay-fragment.ava-dialog-closing { "
               << "animation: ava-overlay-fade-out " << closeAnim.duration << " " << closeAnim.easing
               << " forwards; pointer-events: none; }\n";
    dynamicCss << EmitProjectStateCSS();
    dynamicCss << EmitProjectResponsiveCSS();
    html_ << EmitProjectCssLink(dynamicCss.str());
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

    if (!extraBodyEnd_.empty()) html_ << extraBodyEnd_;
    html_ << "</body>\n";
    html_ << "</html>\n";
    return html_.str();
}

void HTMLRenderer::OnBeginFrame() {
    bodyHtml_.str("");
    bodyHtml_.clear();
    usedFontNames_.clear();
    usedFontNames_.insert(kDefaultCssFontFamily);
}

void HTMLRenderer::OnEndFrame() {
    cachedOutput_ = EmitHTMLHeader() + bodyHtml_.str() + EmitHTMLFooter();
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
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
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
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
    }
    bodyHtml_ << "></div>\n";
}

std::string HTMLRenderer::BuildSvgPathData(const std::vector<render::PathSegment>& segments, bool closed) const {
    std::ostringstream d;
    for (const render::PathSegment& segment : segments) {
        switch (segment.type) {
            case render::PathSegmentType::MoveTo:
                d << "M " << segment.x << " " << segment.y << " ";
                break;
            case render::PathSegmentType::LineTo:
                d << "L " << segment.x << " " << segment.y << " ";
                break;
            case render::PathSegmentType::CubicCurveTo:
                d << "C " << segment.cx1 << " " << segment.cy1 << " "
                  << segment.cx2 << " " << segment.cy2 << " "
                  << segment.x << " " << segment.y << " ";
                break;
            case render::PathSegmentType::Close:
                d << "Z ";
                break;
        }
    }
    if (closed) {
        d << "Z";
    }
    return d.str();
}

void HTMLRenderer::OnDrawPath(
    float x, float y,
    const std::vector<render::PathSegment>& segments,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    bool closed,
    const std::string& clickHandler,
    const std::string& className
) {
    if (segments.empty()) {
        return;
    }

    const bool hasClass = !className.empty();
    bodyHtml_ << "<svg class=\"" << (hasClass ? className : std::string("ava-element")) << "\" "
              << "style=\"position: absolute; left: " << x << "px; top: " << y << "px; "
              << "overflow: visible; opacity: " << currentOpacity_ << "; "
              << GetTransformCSS() << " "
              << GetClipCSS()
              << "\" width=\"0\" height=\"0\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
    }
    bodyHtml_ << "><path d=\"" << BuildSvgPathData(segments, closed) << "\" "
              << "fill=\"" << ColorToHex(fillColor) << "\" "
              << "stroke=\"" << ColorToHex(borderColor) << "\" "
              << "stroke-width=\"" << borderWidth << "\" /></svg>\n";
}

void HTMLRenderer::OnDrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className,
    float maxWidth,
    bool wrap
) {
    const bool hasClass = !className.empty();
    if (hasClass) {
        bodyHtml_ << "<div class=\"" << className << "\" style=\"";
    } else {
        bodyHtml_ << "<div class=\"ava-element ava-text\" style=\"";
        bodyHtml_ << "left: " << x << "px; top: " << y << "px; ";
        if (wrap && maxWidth > 0.0f) {
            bodyHtml_ << "width: " << maxWidth << "px; white-space: normal; overflow-wrap: break-word; ";
        } else if (maxWidth > 0.0f) {
            bodyHtml_ << "width: " << maxWidth << "px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; ";
        } else {
            bodyHtml_ << "white-space: nowrap; ";
        }
        bodyHtml_ << "font-size: " << fontSize << "px; "
              << "font-family: '" << ResolveCssFontFamily(fontName) << "', sans-serif; "
              << "color: " << ColorToHex(color) << "; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
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
              << "font-family: '" << ResolveCssFontFamily(fontName) << "', sans-serif; "
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
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
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
              << "font-family: '" << ResolveCssFontFamily(fontName) << "', sans-serif; "
              << "color: " << ColorToHex(color) << "; ";
    }
    bodyHtml_ << "opacity: " << currentOpacity_ << "; "
          << GetTransformCSS() << " "
          << GetClipCSS()
          << "\"";
    if (!clickHandler.empty()) {
        bodyHtml_ << " data-event=\"click\" data-handler=\"" << EscapeHtmlAttr(clickHandler) << "\"";
    }
    bodyHtml_ << ">" << (text ? text : "") << "</a>\n";
}

}
}