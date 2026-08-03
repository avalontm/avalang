#ifndef AVA_UI_IRENDER_COMMAND_SINK_H
#define AVA_UI_IRENDER_COMMAND_SINK_H

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
        const std::string& className = std::string()
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

    static std::unique_ptr<IRenderCommandSink> Create();
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_IRENDER_COMMAND_SINK_H
