#ifndef AVA_UI_HTML_RENDERER_H
#define AVA_UI_HTML_RENDERER_H

#include "renderer/BaseRenderer.h"
#include "theme/ProjectStyleOverrides.h"
#include "theme/ProjectAnimationOverrides.h"
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace avalang {
namespace ui {

class HTMLRenderer final : public BaseRenderer {
public:
    HTMLRenderer(int width, int height);
    ~HTMLRenderer() override = default;

    HTMLRenderer(const HTMLRenderer&) = delete;
    HTMLRenderer& operator=(const HTMLRenderer&) = delete;

    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetExtraHead(std::string html) { extraHead_ = std::move(html); }
    void SetExtraBodyEnd(std::string html) { extraBodyEnd_ = std::move(html); }
    void SetFragmentOnly(bool fragmentOnly) { fragmentOnly_ = fragmentOnly; }

    // Non-owning, same convention as ITheme* elsewhere (see
    // theme::ProjectTheme's base_ member) -- caller (the render
    // pipeline) keeps the ProjectStyleSheet alive for this renderer's
    // one frame. When set and it declares any `style <type>:hover` /
    // `:focus` / `:active` / `:disabled` blocks, EmitHTMLHeader emits
    // matching CSS rules for the handful of control types that get a
    // stable class (see EmitProjectStateCSS in the .cpp). Null (the
    // default) or a sheet with no state blocks is a harmless no-op.
    void SetProjectStyles(const theme::ProjectStyleSheet* styles) { projectStyles_ = styles; }

    // Non-owning, same convention as SetProjectStyles above -- caller
    // (the render pipeline) keeps the ProjectAnimationSheet alive for
    // this renderer's one frame. When set and it declares a
    // `dialog:open` and/or `dialog:close` block (see
    // theme/ProjectAnimationOverrides.h), EmitHTMLHeader uses those
    // field(s) in place of the built-in 160ms ease-out/ease-in dialog
    // fade -- any field the project didn't set keeps its built-in
    // default. Null (the default) or a sheet with no `dialog:open`/
    // `dialog:close` blocks is a harmless no-op (built-in fade only).
    void SetProjectAnimations(const theme::ProjectAnimationSheet* animations) {
        projectAnimations_ = animations;
    }

    const std::string& Title() const { return title_; }

    const char* GetOutput() const override;

    // Fase 24 -- ScrollView. HTML is the only backend that can host a
    // real nested scrolling coordinate space today (a DOM element with
    // its own `overflow: auto`); GdiRenderer has no such concept yet.
    bool SupportsScrollRegions() const override { return true; }

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
    // Body markup only (everything between the .ava-viewport div and its
    // close). The <head>, including @font-face rules, can't be known
    // until AFTER the body is drawn -- OnDrawText/OnDrawButton/OnDrawLink
    // only find out which font names are actually used as they run -- so
    // the full document is assembled in OnEndFrame from bodyHtml_ +
    // whatever fonts_ collected, instead of streaming a <head> up front.
    std::stringstream bodyHtml_;
    std::string cachedOutput_;
    bool outputDirty_;
    std::vector<std::string> styleRules_;
    std::string title_ = "AvaHost App";
    std::string extraHead_;
    std::string extraBodyEnd_;
    bool fragmentOnly_ = false;

    // Font family names (as written into `font-family:` CSS) seen while
    // drawing this frame's text/button/link elements. One @font-face
    // rule per name is emitted at OnEndFrame, backed by whatever
    // FontRegistry actually measured that name against -- see
    // EmitFontFaceRules. A std::set keeps them de-duplicated and in a
    // stable order without an extra lookup structure.
    std::set<std::string> usedFontNames_;

    // Resolves `fontName` to the CSS `font-family` value to write
    // inline (quotes it, falls back to a canonical name for an empty/
    // null fontName) and records it in usedFontNames_ so OnEndFrame
    // knows to emit a matching @font-face rule.
    std::string ResolveCssFontFamily(const char* fontName);

    std::string EmitHTMLHeader();
    std::string EmitHTMLFooter();
    std::string EmitFontFaceRules() const;
    // Builds the `.ava-<type>:hover { ... }` etc. CSS rules from
    // projectStyles_ -- see the .cpp for the type->class map and why
    // every declaration is `!important`. Returns "" when
    // projectStyles_ is null or declares no state blocks.
    std::string EmitProjectStateCSS() const;
    void EmitCSSFromState();

    std::string ColorToHex(const Color& c) const;
    std::string GetTransformCSS() const;
    std::string GetClipCSS() const;

    const theme::ProjectStyleSheet* projectStyles_ = nullptr;
    const theme::ProjectAnimationSheet* projectAnimations_ = nullptr;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_HTML_RENDERER_H
