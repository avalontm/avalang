#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "render_tree/PathGeometry.h"
#include "render_tree/ComboBoxData.h"
#include "Fwd.h"

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
    DrawPath,
    DrawInput,
    DrawCheckBox,
    DrawRadioButton,
    DrawComboBox,
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
    } pushClip;

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
        ComponentId compId = 0;
        std::string avaType;
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
        ComponentId compId = 0;
        std::string avaType;
    } drawLink;

    struct {
        float x, y;
        std::vector<render::PathSegment> segments;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        bool closed;
        std::string clickHandler;
        std::string className;
    } drawPath;

    struct {
        float x, y, width, height;
        std::string text;
        std::string placeholder;
        float fontSize;
        const char* fontName;
        Color textColor;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        float borderRadius;
        bool disabled;
        bool focused;
        bool hovered;
        int caretIndex;
        int selectionStart;
        int selectionEnd;
        std::string imeComposition;
        int imeCompositionCursor;
        std::string clickHandler;
        std::string className;
        ComponentId compId = 0;
        std::string avaType;
    } drawInput;

    struct {
        float x, y, width, height;
        std::string text;
        float fontSize;
        const char* fontName;
        Color textColor;
        Color boxFillColor;
        Color boxBorderColor;
        float borderWidth;
        float borderRadius;
        bool checked;
        bool disabled;
        bool focused;
        bool hovered;
        std::string clickHandler;
        std::string className;
        ComponentId compId = 0;
        std::string avaType;
    } drawCheckBox;

    struct {
        float x, y, width, height;
        std::string text;
        float fontSize;
        const char* fontName;
        Color textColor;
        Color boxFillColor;
        Color boxBorderColor;
        float borderWidth;
        bool selected;
        bool disabled;
        bool focused;
        bool hovered;
        std::string clickHandler;
        std::string className;
        ComponentId compId = 0;
        std::string avaType;
    } drawRadioButton;

    struct {
        float x, y, width, height;
        std::vector<render::ComboBoxItem> items;
        float fontSize;
        const char* fontName;
        Color textColor;
        Color fillColor;
        Color borderColor;
        float borderWidth;
        float borderRadius;
        bool disabled;
        bool focused;
        bool hovered;
        bool open;
        std::string clickHandler;
        std::string className;
        ComponentId compId = 0;
        std::string avaType;
    } drawComboBox;
};

}
}