#ifndef AVA_UI_BASE_RENDERER_H
#define AVA_UI_BASE_RENDERER_H

#include "renderer/IRenderer.h"
#include <stack>
#include <memory>
#include <string>

namespace avalang {
namespace ui {

class BaseRenderer : public IRenderer {
public:
    BaseRenderer(int width, int height);
    ~BaseRenderer() override = default;

    void BeginFrame() override;
    void EndFrame() override;

    int GetWidth() const override { return width_; }
    int GetHeight() const override { return height_; }
    void SetViewport(int width, int height) override;

    void ProcessCommands(const std::vector<RenderCommand>& commands) override;

    void DrawRectangle(
        float x, float y, float width, float height,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        float borderRadius = 0.0f,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) override;

    void DrawEllipse(
        float cx, float cy, float rx, float ry,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) override;

    void DrawText(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) override;

    void DrawImage(
        float x, float y, float width, float height,
        const char* imagePath
    ) override;

    void DrawHtmlFragment(std::string html) override;

    void DrawButton(
        float x, float y, float width, float height,
        const char* text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) override;

    void PushClipRect(float x, float y, float width, float height) override;
    void PopClipRect() override;

    void Translate(float x, float y) override;
    void Scale(float sx, float sy) override;
    void Rotate(float angle) override;

    void PushTransform() override;
    void PopTransform() override;

    void SetOpacity(float opacity) override;
    void ResetTransform() override;

protected:
    int width_, height_;
    float currentOpacity_;

    struct TransformState {
        float tx, ty;
        float sx, sy;
        float rotation;
    };

    struct ClipRect {
        float x, y, w, h;
    };

    TransformState currentTransform_;
    std::stack<TransformState> transformStack_;
    std::stack<ClipRect> clipStack_;

    virtual void OnDrawRectangle(
        float x, float y, float width, float height,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        float borderRadius,
        const std::string& clickHandler,
        const std::string& className
    ) = 0;

    virtual void OnDrawEllipse(
        float cx, float cy, float rx, float ry,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        const std::string& clickHandler,
        const std::string& className
    ) = 0;

    virtual void OnDrawText(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& clickHandler,
        const std::string& className
    ) = 0;

    virtual void OnDrawImage(
        float x, float y, float width, float height,
        const char* imagePath
    ) = 0;

    virtual void OnDrawHtmlFragment(const std::string& html) = 0;

    virtual void OnDrawButton(
        float x, float y, float width, float height,
        const char* text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled,
        const std::string& clickHandler,
        const std::string& className
    ) = 0;

    virtual void OnBeginFrame() {}
    virtual void OnEndFrame() {}
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_BASE_RENDERER_H
