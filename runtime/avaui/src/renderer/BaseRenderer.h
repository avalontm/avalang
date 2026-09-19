#pragma once

#include "renderer/IRenderer.h"
#include "commands/RenderCommandBounds.h"
#include "Export.h"

#include <memory>
#include <stack>
#include <string>

namespace avalang {
namespace ui {

struct RectGeometry {
    float x, y, width, height;
    std::string clickHandler;
    std::string className;
};

class AVA_UI_API BaseRenderer : public IRenderer {
public:
    BaseRenderer(int width, int height);
    ~BaseRenderer() override = default;

    void BeginFrame() override;
    void EndFrame() override;

    int GetWidth() const override { return width_; }
    int GetHeight() const override { return height_; }
    void SetViewport(int width, int height) override;

    void ProcessCommands(const std::vector<RenderCommand>& commands) override;

    void ProcessCommandsIncremental(
        const std::vector<RenderCommand>& commands,
        const std::vector<DirtyRect>& dirtyRegions
    ) override;

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
        const std::string& className = std::string(),
        float maxWidth = -1.0f,
        bool wrap = false
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
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) override;

    void DrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) override;

    void DrawPath(
        float x, float y,
        const std::vector<render::PathSegment>& segments,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        bool closed = false,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) override;

    void DrawInput(
        float x, float y, float width, float height,
        const std::string& text,
        const std::string& placeholder,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled, bool focused, bool hovered,
        int caretIndex = -1, int selectionStart = -1, int selectionEnd = -1,
        const std::string& imeComposition = std::string(), int imeCompositionCursor = 0,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) override;

    void DrawCheckBox(
        float x, float y, float width, float height,
        const std::string& text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& boxFillColor,
        const Color& boxBorderColor, float borderWidth, float borderRadius,
        bool checked, bool disabled, bool focused, bool hovered,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) override;

    void DrawRadioButton(
        float x, float y, float width, float height,
        const std::string& text,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& boxFillColor,
        const Color& boxBorderColor, float borderWidth,
        bool selected, bool disabled, bool focused, bool hovered,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) override;

    void DrawComboBox(
        float x, float y, float width, float height,
        const std::vector<render::ComboBoxItem>& items,
        float fontSize, const char* fontName,
        const Color& textColor,
        const Color& fillColor,
        const Color& borderColor, float borderWidth, float borderRadius,
        bool disabled, bool focused, bool hovered, bool open,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
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

    virtual void OnDrawRectangleBatch(
        const std::vector<RectGeometry>& rects,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        float borderRadius
    ) {
        for (const RectGeometry& rect : rects) {
            OnDrawRectangle(rect.x, rect.y, rect.width, rect.height, fillColor, borderColor,
                             borderWidth, borderRadius, rect.clickHandler, rect.className);
        }
    }

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
        const std::string& className,
        float maxWidth,
        bool wrap
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
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) = 0;

    virtual void OnDrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) = 0;

    virtual void OnDrawPath(
        float x, float y,
        const std::vector<render::PathSegment>& segments,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        bool closed,
        const std::string& clickHandler,
        const std::string& className
    ) {}

    virtual void OnDrawInput(
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
    ) {}

    virtual void OnDrawCheckBox(
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
    ) {}

    virtual void OnDrawRadioButton(
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
    ) {}

    virtual void OnDrawComboBox(
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
    ) {}

    virtual void OnBeginFrame() {}
    virtual void OnEndFrame() {}

    virtual void OnPushClipRect(float x, float y, float width, float height) {}
    virtual void OnPopClipRect() {}

    void ApplyTransform(float& x, float& y) const;

private:
    void DispatchCommand(const RenderCommand& cmd);
    RectGeometry ToRectGeometry(const RenderCommand& cmd) const;
};

}
}