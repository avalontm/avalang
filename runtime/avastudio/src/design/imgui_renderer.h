#pragma once

#include "imgui.h"
#include "renderer/BaseRenderer.h"

namespace avalang::ui {

class ImGuiRenderer : public BaseRenderer {
public:
    ImGuiRenderer(int width, int height);
    ~ImGuiRenderer() override = default;

    void SetTarget(ImDrawList* drawList, ImVec2 origin);

protected:
    void OnDrawRectangle(float x, float y, float width, float height,
                         const Color& fillColor, const Color& borderColor,
                         float borderWidth, float borderRadius,
                         const std::string& clickHandler,
                         const std::string& className) override;
    void OnDrawEllipse(float cx, float cy, float rx, float ry,
                       const Color& fillColor, const Color& borderColor,
                       float borderWidth, const std::string& clickHandler,
                       const std::string& className) override;
    void OnDrawText(float x, float y, const char* text, float fontSize,
                    const char* fontName, const Color& color,
                    const std::string& clickHandler,
                    const std::string& className,
                    float maxWidth, bool wrap) override;
    void OnDrawImage(float x, float y, float width, float height,
                     const char* imagePath) override;
    void OnDrawHtmlFragment(const std::string& html) override;
    void OnDrawButton(float x, float y, float width, float height,
                      const char* text, float fontSize, const char* fontName,
                      const Color& textColor, const Color& fillColor,
                      const Color& borderColor, float borderWidth,
                      float borderRadius, bool disabled,
                      const std::string& clickHandler,
                      const std::string& className) override;
    void OnDrawLink(float x, float y,
                    const char* text, float fontSize, const char* fontName,
                    const Color& color, const std::string& href,
                    const std::string& clickHandler,
                    const std::string& className) override;

private:
    ImDrawList* drawList_ = nullptr;
    ImVec2 origin_{0, 0};

    ImVec2 P(float x, float y) const { return ImVec2(origin_.x + x, origin_.y + y); }
    static ImU32 ToImU32(const Color& c) { return IM_COL32(c.r, c.g, c.b, c.a); }
};

} // namespace avalang::ui
