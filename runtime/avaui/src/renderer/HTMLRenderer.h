#ifndef AVA_UI_HTML_RENDERER_H
#define AVA_UI_HTML_RENDERER_H

#include "renderer/BaseRenderer.h"
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
        const std::string& className
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
    std::stringstream html_;
    std::string cachedOutput_;
    bool outputDirty_;
    std::vector<std::string> styleRules_;
    std::string title_ = "AvaHost App";
    std::string extraHead_;
    std::string extraBodyEnd_;
    bool fragmentOnly_ = false;

    void EmitHTMLHeader();
    void EmitHTMLFooter();
    void EmitCSSFromState();

    std::string ColorToHex(const Color& c) const;
    std::string GetTransformCSS() const;
    std::string GetClipCSS() const;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_HTML_RENDERER_H
