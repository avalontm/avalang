#pragma once

#include "renderer/BaseRenderer.h"

#include <cstdint>

namespace avalang {
namespace ui {

class AndroidCanvasRenderer final : public BaseRenderer {
public:
    AndroidCanvasRenderer(int width, int height);
    ~AndroidCanvasRenderer() override = default;

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
        const std::string& className,
        float maxWidth,
        bool wrap
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
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawLink(
        float x, float y,
        const char* text,
        float fontSize, const char* fontName,
        const Color& color,
        const std::string& href,
        const std::string& clickHandler,
        const std::string& className,
        ComponentId compId,
        const std::string& avaType
    ) override;

    void OnDrawCheckBox(
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
    ) override;

    void OnDrawRadioButton(
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
    ) override;

    void OnDrawInput(
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
    ) override;

    void OnDrawComboBox(
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
    ) override;

    void OnBeginFrame() override;
    void OnEndFrame() override;

    void OnPushClipRect(float x, float y, float width, float height) override;
    void OnPopClipRect() override;

private:
    static uint32_t ToArgb(const Color& c);
};

}
}