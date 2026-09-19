#pragma once

#include "commands/IRenderCommandSink.h"
#include "Export.h"
#include <vector>
#include <stack>
#include <string>

namespace avalang {
namespace ui {

struct ClipRect {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
    bool enabled = true;
};

class AVA_UI_API RenderCommandSink final : public IRenderCommandSink {
public:
    RenderCommandSink();
    ~RenderCommandSink() override = default;

    RenderCommandSink(const RenderCommandSink&) = delete;
    RenderCommandSink& operator=(const RenderCommandSink&) = delete;

    void Emit(const RenderCommand& cmd) override;
    void BeginFrame() override;
    void EndFrame() override;

    void PushClipRect(float x, float y, float width, float height) override;
    void PopClipRect() override;

    void Translate(float x, float y) override;
    void Scale(float sx, float sy) override;
    void Rotate(float angle) override;

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

    const std::vector<RenderCommand>& GetCommands() const { return commands_; }

    const std::stack<ClipRect>& GetClipStack() const { return clipStack_; }

private:
    std::vector<RenderCommand> commands_;
    std::stack<ClipRect> clipStack_;
};

}
}