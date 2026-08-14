#ifndef AVA_UI_IRENDERER_H
#define AVA_UI_IRENDERER_H

#include "commands/RenderCommand.h"
#include "Export.h"
#include <memory>
#include <vector>
#include <string>

namespace avalang {
namespace ui {

class AVA_UI_API IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void BeginFrame() = 0;

    virtual void EndFrame() = 0;

    virtual int GetWidth() const = 0;

    virtual int GetHeight() const = 0;

    virtual void SetViewport(int width, int height) = 0;

    virtual void ProcessCommands(const std::vector<RenderCommand>& commands) = 0;

    virtual void DrawRectangle(
        float x, float y, float width, float height,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        float borderRadius = 0.0f,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) = 0;

    virtual void DrawEllipse(
        float cx, float cy, float rx, float ry,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) = 0;

    virtual void DrawText(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        // <=0 means no wrap (unchanged nowrap behavior); >0 word-wraps
        // `text` to that width in px before drawing. See
        // commands/RenderCommand.h's drawText.maxWidth.
        float maxWidth = -1.0f
    ) = 0;

    virtual void DrawImage(
        float x, float y, float width, float height,
        const char* imagePath
    ) = 0;

    virtual void DrawHtmlFragment(std::string html) = 0;

    virtual void DrawButton(
        float x, float y, float width, float height,
        const char* text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) = 0;

    virtual void DrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) = 0;

    virtual void DrawPath(const char* pathData) {}

    virtual void PushClipRect(float x, float y, float width, float height) = 0;

    virtual void PopClipRect() = 0;

    virtual void Translate(float x, float y) = 0;

    virtual void Scale(float sx, float sy) = 0;

    virtual void Rotate(float angle) = 0;

    virtual void PushTransform() = 0;

    virtual void PopTransform() = 0;

    virtual void SetOpacity(float opacity) = 0;

    virtual void ResetTransform() = 0;

    virtual const char* GetOutput() const { return nullptr; }

    // Fase 24 -- ScrollView. Whether this renderer can host a real
    // nested scrolling coordinate space (a DOM element with its own
    // `overflow: auto` and its own containing block for descendants).
    // Only HTMLRenderer overrides this to true today. GdiRenderer
    // (desktop) has no such concept yet, so SceneCommandWalker falls
    // back to drawing a ScrollView's children inline, unclipped, exactly
    // like a plain Container there -- same "web works, desktop stub" gap
    // Link/ComboBox interactivity/TextBox editing already have.
    virtual bool SupportsScrollRegions() const { return false; }

    static std::unique_ptr<IRenderer> Create(const char* backend = "html",
                                             int width = 800, int height = 600);
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_IRENDERER_H
