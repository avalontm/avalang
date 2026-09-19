#include "commands/RenderCommandSink.h"
#include <cstring>

namespace avalang {
namespace ui {

RenderCommandSink::RenderCommandSink() = default;

void RenderCommandSink::Emit(const RenderCommand& cmd) {
    commands_.push_back(cmd);
}

void RenderCommandSink::BeginFrame() {
    commands_.clear();
    while (!clipStack_.empty()) {
        clipStack_.pop();
    }
}

void RenderCommandSink::EndFrame() {
}

void RenderCommandSink::PushClipRect(float x, float y, float width, float height) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::PushClip;
    cmd.pushClip.x = x;
    cmd.pushClip.y = y;
    cmd.pushClip.width = width;
    cmd.pushClip.height = height;
    clipStack_.push(ClipRect{x, y, width, height, true});
    Emit(cmd);
}

void RenderCommandSink::PopClipRect() {
    if (!clipStack_.empty()) {
        clipStack_.pop();
    }
    RenderCommand cmd;
    cmd.type = RenderCommandType::PopClip;
    Emit(cmd);
}

void RenderCommandSink::Translate(float x, float y) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Translate;
    cmd.transform.x = x;
    cmd.transform.y = y;
    Emit(cmd);
}

void RenderCommandSink::Scale(float sx, float sy) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Scale;
    cmd.transform.sx = sx;
    cmd.transform.sy = sy;
    Emit(cmd);
}

void RenderCommandSink::Rotate(float angle) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::Rotate;
    cmd.transform.angle = angle;
    Emit(cmd);
}

void RenderCommandSink::DrawRectangle(
    float x, float y, float width, float height,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    float borderRadius,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawRectangle;
    cmd.drawRect.x = x;
    cmd.drawRect.y = y;
    cmd.drawRect.width = width;
    cmd.drawRect.height = height;
    cmd.drawRect.fillColor = fillColor;
    cmd.drawRect.borderColor = borderColor;
    cmd.drawRect.borderWidth = borderWidth;
    cmd.drawRect.borderRadius = borderRadius;
    cmd.drawRect.clickHandler = clickHandler;
    cmd.drawRect.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawEllipse(
    float cx, float cy, float rx, float ry,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawEllipse;
    cmd.drawEllipse.cx = cx;
    cmd.drawEllipse.cy = cy;
    cmd.drawEllipse.rx = rx;
    cmd.drawEllipse.ry = ry;
    cmd.drawEllipse.fillColor = fillColor;
    cmd.drawEllipse.borderColor = borderColor;
    cmd.drawEllipse.borderWidth = borderWidth;
    cmd.drawEllipse.clickHandler = clickHandler;
    cmd.drawEllipse.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawText(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& clickHandler,
    const std::string& className,
    float maxWidth,
    bool wrap
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawText;
    cmd.drawText.x = x;
    cmd.drawText.y = y;
    cmd.drawText.text = text;
    cmd.drawText.fontSize = fontSize;
    cmd.drawText.fontName = fontName;
    cmd.drawText.color = color;
    cmd.drawText.clickHandler = clickHandler;
    cmd.drawText.className = className;
    cmd.drawText.maxWidth = maxWidth;
    cmd.drawText.wrap = wrap;
    Emit(cmd);
}

void RenderCommandSink::DrawImage(
    float x, float y, float width, float height,
    const char* imagePath
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawImage;
    cmd.drawImage.x = x;
    cmd.drawImage.y = y;
    cmd.drawImage.width = width;
    cmd.drawImage.height = height;
    cmd.drawImage.imagePath = imagePath;
    Emit(cmd);
}

void RenderCommandSink::DrawHtmlFragment(std::string html) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawHtmlFragment;
    cmd.drawHtml.html = std::move(html);
    Emit(cmd);
}

void RenderCommandSink::DrawButton(
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
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawButton;
    cmd.drawButton.x = x;
    cmd.drawButton.y = y;
    cmd.drawButton.width = width;
    cmd.drawButton.height = height;
    cmd.drawButton.text = text;
    cmd.drawButton.fontSize = fontSize;
    cmd.drawButton.fontName = fontName;
    cmd.drawButton.textColor = textColor;
    cmd.drawButton.fillColor = fillColor;
    cmd.drawButton.borderColor = borderColor;
    cmd.drawButton.borderWidth = borderWidth;
    cmd.drawButton.borderRadius = borderRadius;
    cmd.drawButton.disabled = disabled;
    cmd.drawButton.clickHandler = clickHandler;
    cmd.drawButton.className = className;
    cmd.drawButton.compId = compId;
    cmd.drawButton.avaType = avaType;
    Emit(cmd);
}

void RenderCommandSink::DrawLink(
    float x, float y,
    const char* text,
    float fontSize, const char* fontName,
    const Color& color,
    const std::string& href,
    const std::string& clickHandler,
    const std::string& className,
    ComponentId compId,
    const std::string& avaType
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawLink;
    cmd.drawLink.x = x;
    cmd.drawLink.y = y;
    cmd.drawLink.text = text;
    cmd.drawLink.fontSize = fontSize;
    cmd.drawLink.fontName = fontName;
    cmd.drawLink.color = color;
    cmd.drawLink.href = href;
    cmd.drawLink.clickHandler = clickHandler;
    cmd.drawLink.className = className;
    cmd.drawLink.compId = compId;
    cmd.drawLink.avaType = avaType;
    Emit(cmd);
}

void RenderCommandSink::DrawPath(
    float x, float y,
    const std::vector<render::PathSegment>& segments,
    const Color& fillColor,
    const Color& borderColor, float borderWidth,
    bool closed,
    const std::string& clickHandler,
    const std::string& className
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawPath;
    cmd.drawPath.x = x;
    cmd.drawPath.y = y;
    cmd.drawPath.segments = segments;
    cmd.drawPath.fillColor = fillColor;
    cmd.drawPath.borderColor = borderColor;
    cmd.drawPath.borderWidth = borderWidth;
    cmd.drawPath.closed = closed;
    cmd.drawPath.clickHandler = clickHandler;
    cmd.drawPath.className = className;
    Emit(cmd);
}

void RenderCommandSink::DrawInput(
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
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawInput;
    cmd.drawInput.x = x;
    cmd.drawInput.y = y;
    cmd.drawInput.width = width;
    cmd.drawInput.height = height;
    cmd.drawInput.text = text;
    cmd.drawInput.placeholder = placeholder;
    cmd.drawInput.fontSize = fontSize;
    cmd.drawInput.fontName = fontName;
    cmd.drawInput.textColor = textColor;
    cmd.drawInput.fillColor = fillColor;
    cmd.drawInput.borderColor = borderColor;
    cmd.drawInput.borderWidth = borderWidth;
    cmd.drawInput.borderRadius = borderRadius;
    cmd.drawInput.disabled = disabled;
    cmd.drawInput.focused = focused;
    cmd.drawInput.hovered = hovered;
    cmd.drawInput.caretIndex = caretIndex;
    cmd.drawInput.selectionStart = selectionStart;
    cmd.drawInput.selectionEnd = selectionEnd;
    cmd.drawInput.imeComposition = imeComposition;
    cmd.drawInput.imeCompositionCursor = imeCompositionCursor;
    cmd.drawInput.clickHandler = clickHandler;
    cmd.drawInput.className = className;
    cmd.drawInput.compId = compId;
    cmd.drawInput.avaType = avaType;
    Emit(cmd);
}

void RenderCommandSink::DrawCheckBox(
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
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawCheckBox;
    cmd.drawCheckBox.x = x;
    cmd.drawCheckBox.y = y;
    cmd.drawCheckBox.width = width;
    cmd.drawCheckBox.height = height;
    cmd.drawCheckBox.text = text;
    cmd.drawCheckBox.fontSize = fontSize;
    cmd.drawCheckBox.fontName = fontName;
    cmd.drawCheckBox.textColor = textColor;
    cmd.drawCheckBox.boxFillColor = boxFillColor;
    cmd.drawCheckBox.boxBorderColor = boxBorderColor;
    cmd.drawCheckBox.borderWidth = borderWidth;
    cmd.drawCheckBox.borderRadius = borderRadius;
    cmd.drawCheckBox.checked = checked;
    cmd.drawCheckBox.disabled = disabled;
    cmd.drawCheckBox.focused = focused;
    cmd.drawCheckBox.hovered = hovered;
    cmd.drawCheckBox.clickHandler = clickHandler;
    cmd.drawCheckBox.className = className;
    cmd.drawCheckBox.compId = compId;
    cmd.drawCheckBox.avaType = avaType;
    Emit(cmd);
}

void RenderCommandSink::DrawRadioButton(
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
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawRadioButton;
    cmd.drawRadioButton.x = x;
    cmd.drawRadioButton.y = y;
    cmd.drawRadioButton.width = width;
    cmd.drawRadioButton.height = height;
    cmd.drawRadioButton.text = text;
    cmd.drawRadioButton.fontSize = fontSize;
    cmd.drawRadioButton.fontName = fontName;
    cmd.drawRadioButton.textColor = textColor;
    cmd.drawRadioButton.boxFillColor = boxFillColor;
    cmd.drawRadioButton.boxBorderColor = boxBorderColor;
    cmd.drawRadioButton.borderWidth = borderWidth;
    cmd.drawRadioButton.selected = selected;
    cmd.drawRadioButton.disabled = disabled;
    cmd.drawRadioButton.focused = focused;
    cmd.drawRadioButton.hovered = hovered;
    cmd.drawRadioButton.clickHandler = clickHandler;
    cmd.drawRadioButton.className = className;
    cmd.drawRadioButton.compId = compId;
    cmd.drawRadioButton.avaType = avaType;
    Emit(cmd);
}

void RenderCommandSink::DrawComboBox(
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
) {
    RenderCommand cmd;
    cmd.type = RenderCommandType::DrawComboBox;
    cmd.drawComboBox.x = x;
    cmd.drawComboBox.y = y;
    cmd.drawComboBox.width = width;
    cmd.drawComboBox.height = height;
    cmd.drawComboBox.items = items;
    cmd.drawComboBox.fontSize = fontSize;
    cmd.drawComboBox.fontName = fontName;
    cmd.drawComboBox.textColor = textColor;
    cmd.drawComboBox.fillColor = fillColor;
    cmd.drawComboBox.borderColor = borderColor;
    cmd.drawComboBox.borderWidth = borderWidth;
    cmd.drawComboBox.borderRadius = borderRadius;
    cmd.drawComboBox.disabled = disabled;
    cmd.drawComboBox.focused = focused;
    cmd.drawComboBox.hovered = hovered;
    cmd.drawComboBox.open = open;
    cmd.drawComboBox.clickHandler = clickHandler;
    cmd.drawComboBox.className = className;
    cmd.drawComboBox.compId = compId;
    cmd.drawComboBox.avaType = avaType;
    Emit(cmd);
}

std::unique_ptr<IRenderCommandSink> IRenderCommandSink::Create() {
    return std::make_unique<RenderCommandSink>();
}

}
}