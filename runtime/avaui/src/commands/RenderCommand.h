#ifndef AVA_UI_RENDER_COMMAND_H
#define AVA_UI_RENDER_COMMAND_H

#include <cstdint>
#include <memory>
#include <string>

namespace avalang {
namespace ui {

enum class RenderCommandType : std::uint8_t {
    DrawRectangle,
    DrawText,
    DrawImage,
    PushClip,
    PopClip,
    Translate,
    Scale,
    Rotate,
    DrawEllipse,
    DrawHtmlFragment,
    DrawButton,
    DrawLink,
};

struct Color {
    std::uint8_t r, g, b, a;
};

struct RenderCommand {
    RenderCommandType type;

    struct {
        float x, y, width, height;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        float borderRadius;
        std::string clickHandler;
        std::string className;
    } drawRect;

    struct {
        float cx, cy, rx, ry;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        std::string clickHandler;
        std::string className;
    } drawEllipse;

    struct {
        float x, y;
        const char* text;
        float fontSize;
        const char* fontName;
        Color color;
        std::string clickHandler;
        std::string className;
        float maxWidth = -1.0f;
        bool wrap = false;
    } drawText;

    struct {
        float x, y, width, height;
        const char* imagePath;
    } drawImage;

    struct {
        std::string html;
    } drawHtml;

    struct {
        float x, y;
        float sx, sy;
        float angle;
    } transform;

    struct {
        float x, y, width, height;
        const char* text;
        float fontSize;
        const char* fontName;
        Color textColor;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        float borderRadius;
        bool disabled;
        std::string clickHandler;
        std::string className;
    } drawButton;

    struct {
        float x, y;
        const char* text;
        float fontSize;
        const char* fontName;
        Color color;
        std::string href;
        std::string clickHandler;
        std::string className;
    } drawLink;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_RENDER_COMMAND_H
