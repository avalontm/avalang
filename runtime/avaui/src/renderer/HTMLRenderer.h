#pragma once

#include "renderer/BaseRenderer.h"
#include "render_tree/PathGeometry.h"
#include "theme/ProjectAnimationOverrides.h"
#include "theme/ProjectStyleOverrides.h"

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

    void SetProjectStyles(const theme::ProjectStyleSheet* styles) { projectStyles_ = styles; }

    void SetProjectAnimations(const theme::ProjectAnimationSheet* animations) {
        projectAnimations_ = animations;
    }

    void SetWwwRootDir(std::string dir) { wwwRootDir_ = std::move(dir); }

    const std::string& Title() const { return title_; }

    const char* GetOutput() const override;

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
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawPath(
        float x, float y,
        const std::vector<render::PathSegment>& segments,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        bool closed,
        const std::string& clickHandler,
        const std::string& className
    ) override;

    void OnDrawInput(
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
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawCheckBox(
        float x, float y, float width, float height,
        const std::string& text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& boxFillColor,
        const Color& boxBorderColor, float borderWidth, float borderRadius,
        bool checked, bool disabled, bool focused, bool hovered,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawRadioButton(
        float x, float y, float width, float height,
        const std::string& text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& boxFillColor,
        const Color& boxBorderColor, float borderWidth,
        bool selected, bool disabled, bool focused, bool hovered,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawComboBox(
        float x, float y, float width, float height,
        const std::vector<render::ComboBoxItem>& items,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled, bool focused, bool hovered, bool open,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnBeginFrame() override;
    void OnEndFrame() override;

private:
    std::stringstream bodyHtml_;
    std::string cachedOutput_;
    std::string title_ = "AvaHost App";
    std::string extraHead_;
    std::string extraBodyEnd_;
    bool fragmentOnly_ = false;

    std::set<std::string> usedFontNames_;

    std::string ResolveCssFontFamily(const char* fontName);

    std::string EmitHTMLHeader();
    std::string EmitHTMLFooter();
    std::string EmitFontFaceRules() const;
    std::string EmitStaticBaseCssLink() const;
    std::string EmitProjectStateCSS() const;
    std::string EmitProjectResponsiveCSS() const;
    std::string EmitProjectCssLink(const std::string& dynamicCss) const;
    void EmitCSSFromState();

    std::string ColorToHex(const Color& c) const;
    std::string BuildSvgPathData(const std::vector<render::PathSegment>& segments, bool closed) const;
    std::string GetTransformCSS() const;
    std::string GetClipCSS() const;
    void AppendPositionStyle(float x, float y, float width, float height);

    const theme::ProjectStyleSheet* projectStyles_ = nullptr;
    const theme::ProjectAnimationSheet* projectAnimations_ = nullptr;
    std::string wwwRootDir_;
};

}
}