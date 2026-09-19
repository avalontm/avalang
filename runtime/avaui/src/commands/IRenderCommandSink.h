#pragma once

#include "RenderCommand.h"
#include <memory>
#include <string>

namespace avalang {
namespace ui {

class IRenderCommandSink {
public:
    virtual ~IRenderCommandSink() = default;

    virtual void Emit(const RenderCommand& cmd) = 0;

    virtual void BeginFrame() = 0;

    virtual void EndFrame() = 0;

    virtual void PushClipRect(float x, float y, float width, float height) = 0;

    virtual void PopClipRect() = 0;

    virtual void Translate(float x, float y) = 0;

    virtual void Scale(float sx, float sy) = 0;

    virtual void Rotate(float angle) = 0;

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
        float maxWidth = -1.0f,
        bool wrap = false
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
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) = 0;

    virtual void DrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string(),
        ComponentId compId = 0,
        const std::string& avaType = std::string()
    ) = 0;

    virtual void DrawPath(
        float x, float y,
        const std::vector<render::PathSegment>& segments,
        const Color& fillColor,
        const Color& borderColor, float borderWidth,
        bool closed = false,
        const std::string& clickHandler = std::string(),
        const std::string& className = std::string()
    ) = 0;

    virtual void DrawInput(
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
    ) = 0;

    virtual void DrawCheckBox(
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
    ) = 0;

    virtual void DrawRadioButton(
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
    ) = 0;

    virtual void DrawComboBox(
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
    ) = 0;

    static std::unique_ptr<IRenderCommandSink> Create();
};

}
}