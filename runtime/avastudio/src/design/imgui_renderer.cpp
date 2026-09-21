#include "design/imgui_renderer.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "GLFW/glfw3.h"
#include "stb_image.h"
#include "layout/LayoutProperties.h"

namespace avalang::ui {

namespace {

struct ImageEntry {
    unsigned int texture_id = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, ImageEntry>& ImageCache() {
    static std::unordered_map<std::string, ImageEntry> cache;
    return cache;
}

std::unordered_set<std::string>& FailedImageCache() {
    static std::unordered_set<std::string> failed;
    return failed;
}

const ImageEntry* GetOrLoadImage(const std::string& path) {
    if (path.empty()) return nullptr;
    if (FailedImageCache().count(path) != 0) return nullptr;

    auto& cache = ImageCache();
    const auto cached = cache.find(path);
    if (cached != cache.end()) return &cached->second;

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        FailedImageCache().insert(path);
        return nullptr;
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    ImageEntry entry;
    entry.texture_id = texture_id;
    entry.width = width;
    entry.height = height;
    const auto [it, inserted] = cache.emplace(path, entry);
    return &it->second;
}

}

ImGuiRenderer::ImGuiRenderer(int width, int height) : BaseRenderer(width, height) {}

void ImGuiRenderer::SetTarget(ImDrawList* drawList, ImVec2 origin) {
    drawList_ = drawList;
    origin_ = origin;
}

void ImGuiRenderer::OnDrawRectangle(float x, float y, float width, float height,
                                    const Color& fillColor, const Color& borderColor,
                                    float borderWidth, float borderRadius,
                                    const std::string& clickHandler,
                                    const std::string& className) {
    (void)clickHandler;
    (void)className;
    if (!drawList_) return;

    const ImVec2 p0 = P(x, y);
    const ImVec2 p1 = P(x + width, y + height);
    drawList_->AddRectFilled(p0, p1, ToImU32(fillColor), borderRadius);
    if (borderWidth > 0.0f) {
        drawList_->AddRect(p0, p1, ToImU32(borderColor), borderRadius, 0, borderWidth);
    }
}

void ImGuiRenderer::OnDrawEllipse(float cx, float cy, float rx, float ry,
                                  const Color& fillColor, const Color& borderColor,
                                  float borderWidth, const std::string& clickHandler,
                                  const std::string& className) {
    (void)clickHandler;
    (void)className;
    if (!drawList_) return;

    const ImVec2 center = P(cx, cy);
    const ImVec2 radius(rx, ry);
    drawList_->AddEllipseFilled(center, radius, ToImU32(fillColor));
    if (borderWidth > 0.0f) {
        drawList_->AddEllipse(center, radius, ToImU32(borderColor), 0.0f, 0, borderWidth);
    }
}

void ImGuiRenderer::OnDrawText(float x, float y, const char* text, float fontSize,
                               const char* fontName, const Color& color,
                               const std::string& clickHandler,
                               const std::string& className,
                               float maxWidth, bool wrap) {
    (void)clickHandler;
    (void)className;
    (void)fontName;
    if (!drawList_ || !text) return;

    const float wrapWidth = (wrap && maxWidth > 0.0f) ? maxWidth : 0.0f;
    drawList_->AddText(ImGui::GetFont(), fontSize, P(x, y), ToImU32(color), text,
                        nullptr, wrapWidth);
}

void ImGuiRenderer::OnDrawImage(float x, float y, float width, float height,
                                const char* imagePath) {
    if (!drawList_ || !imagePath) return;

    const ImageEntry* entry = GetOrLoadImage(imagePath);
    if (!entry) return;

    const ImVec2 p0 = P(x, y);
    const ImVec2 p1 = P(x + width, y + height);
    drawList_->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(entry->texture_id)), p0, p1);
}

void ImGuiRenderer::OnDrawInput(float x, float y, float width, float height,
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
                                const std::string& avaType) {
    (void)disabled;
    (void)hovered;
    (void)caretIndex;
    (void)selectionStart;
    (void)selectionEnd;
    (void)imeComposition;
    (void)imeCompositionCursor;
    (void)clickHandler;
    (void)className;
    (void)compId;
    (void)avaType;
    if (!drawList_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius,
                    std::string(), std::string());

    const bool showingPlaceholder = text.empty() && !placeholder.empty();
    const std::string& shown = showingPlaceholder ? placeholder : text;
    if (!shown.empty()) {
        Color displayColor = textColor;
        if (showingPlaceholder) {
            displayColor.r = static_cast<std::uint8_t>((textColor.r + 255) / 2);
            displayColor.g = static_cast<std::uint8_t>((textColor.g + 255) / 2);
            displayColor.b = static_cast<std::uint8_t>((textColor.b + 255) / 2);
        }

        const float paddingX = static_cast<float>(layout::kDefaultInputPaddingX);
        const float textX = x + paddingX;
        const float textY = y + (height - fontSize * 1.2f) / 2.0f;
        const float maxWidth = width - 2.0f * paddingX;

        OnDrawText(textX, textY, shown.c_str(), fontSize, fontName, displayColor,
                   std::string(), std::string(), maxWidth, false);
    }

    if (focused && !disabled) {
        const ImVec2 p0 = P(x - 1.0f, y - 1.0f);
        const ImVec2 p1 = P(x + width + 1.0f, y + height + 1.0f);
        drawList_->AddRect(p0, p1, ToImU32(borderColor), borderRadius, 0, 1.0f);
    }
}

void ImGuiRenderer::OnDrawCheckBox(float x, float y, float width, float height,
                                   const std::string& text,
                                   float fontSize, const char* fontName,
                                   const Color& textColor,
                                   const Color& boxFillColor,
                                   const Color& boxBorderColor, float borderWidth, float borderRadius,
                                   bool checked, bool disabled, bool focused, bool hovered,
                                   const std::string& clickHandler,
                                   const std::string& className,
                                   ComponentId compId,
                                   const std::string& avaType) {
    (void)disabled;
    (void)hovered;
    (void)clickHandler;
    (void)className;
    (void)compId;
    (void)avaType;
    if (!drawList_) return;

    const float defaultBoxSize = static_cast<float>(layout::kDefaultCheckboxBoxSize);
    const float boxSize = std::min(defaultBoxSize, height > 0.0f ? height : defaultBoxSize);
    const float boxY = y + (height - boxSize) / 2.0f;

    OnDrawRectangle(x, boxY, boxSize, boxSize, boxFillColor, boxBorderColor, borderWidth, borderRadius,
                    std::string(), std::string());

    if (checked) {
        const ImU32 markColor = ToImU32(textColor);
        const ImVec2 p0 = P(x + boxSize * 0.2f, boxY + boxSize * 0.55f);
        const ImVec2 p1 = P(x + boxSize * 0.45f, boxY + boxSize * 0.8f);
        const ImVec2 p2 = P(x + boxSize * 0.8f, boxY + boxSize * 0.2f);
        drawList_->AddLine(p0, p1, markColor, 2.0f);
        drawList_->AddLine(p1, p2, markColor, 2.0f);
    }

    if (focused && !disabled) {
        const ImVec2 p0 = P(x - 2.0f, boxY - 2.0f);
        const ImVec2 p1 = P(x + boxSize + 2.0f, boxY + boxSize + 2.0f);
        drawList_->AddRect(p0, p1, ToImU32(boxBorderColor), 0.0f, 0, 1.0f);
    }

    if (!text.empty()) {
        const float labelX = x + boxSize + static_cast<float>(layout::kDefaultCheckboxLabelGap);
        const float labelY = y + (height - fontSize * 1.2f) / 2.0f;
        OnDrawText(labelX, labelY, text.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), -1.0f, false);
    }
}

void ImGuiRenderer::OnDrawRadioButton(float x, float y, float width, float height,
                                      const std::string& text,
                                      float fontSize, const char* fontName,
                                      const Color& textColor,
                                      const Color& boxFillColor,
                                      const Color& boxBorderColor, float borderWidth,
                                      bool selected, bool disabled, bool focused, bool hovered,
                                      const std::string& clickHandler,
                                      const std::string& className,
                                      ComponentId compId,
                                      const std::string& avaType) {
    (void)disabled;
    (void)hovered;
    (void)clickHandler;
    (void)className;
    (void)compId;
    (void)avaType;
    if (!drawList_) return;

    const float defaultBoxSize = static_cast<float>(layout::kDefaultCheckboxBoxSize);
    const float boxSize = std::min(defaultBoxSize, height > 0.0f ? height : defaultBoxSize);
    const float boxY = y + (height - boxSize) / 2.0f;
    const float cx = x + boxSize / 2.0f;
    const float cy = boxY + boxSize / 2.0f;

    OnDrawEllipse(cx, cy, boxSize / 2.0f, boxSize / 2.0f, boxFillColor, boxBorderColor, borderWidth,
                 std::string(), std::string());

    if (selected) {
        const float dotRadius = boxSize * 0.22f;
        drawList_->AddCircleFilled(P(cx, cy), dotRadius, ToImU32(textColor));
    }

    if (focused && !disabled) {
        const ImVec2 p0 = P(x - 2.0f, boxY - 2.0f);
        const ImVec2 p1 = P(x + boxSize + 2.0f, boxY + boxSize + 2.0f);
        drawList_->AddRect(p0, p1, ToImU32(boxBorderColor), boxSize / 2.0f + 2.0f, 0, 1.0f);
    }

    if (!text.empty()) {
        const float labelX = x + boxSize + static_cast<float>(layout::kDefaultCheckboxLabelGap);
        const float labelY = y + (height - fontSize * 1.2f) / 2.0f;
        OnDrawText(labelX, labelY, text.c_str(), fontSize, fontName, textColor,
                   std::string(), std::string(), -1.0f, false);
    }
}

void ImGuiRenderer::OnDrawHtmlFragment(const std::string& html) {
    (void)html;
}

void ImGuiRenderer::OnDrawLink(float x, float y,
                               const char* text, float fontSize, const char* fontName,
                               const Color& color, const std::string& href,
                               const std::string& clickHandler,
                               const std::string& className,
                               ComponentId compId,
                               const std::string& avaType) {

    (void)href;
    (void)clickHandler;
    (void)className;
    (void)fontName;
    (void)compId;
    (void)avaType;
    if (!drawList_ || !text) return;

    const ImU32 col = ToImU32(color);
    const ImVec2 pos = P(x, y);
    drawList_->AddText(ImGui::GetFont(), fontSize, pos, col, text);

    const ImVec2 extent = ImGui::CalcTextSize(text);
    const float underlineY = pos.y + extent.y;
    drawList_->AddLine(ImVec2(pos.x, underlineY), ImVec2(pos.x + extent.x, underlineY), col, 1.0f);
}

void ImGuiRenderer::OnDrawButton(float x, float y, float width, float height,
                                 const char* text, float fontSize, const char* fontName,
                                 const Color& textColor, const Color& fillColor,
                                 const Color& borderColor, float borderWidth,
                                 float borderRadius, bool disabled,
                                 const std::string& clickHandler,
                                 const std::string& className,
                                 ComponentId compId,
                                 const std::string& avaType) {
    (void)disabled;
    (void)compId;
    (void)avaType;
    if (!drawList_) return;

    OnDrawRectangle(x, y, width, height, fillColor, borderColor, borderWidth, borderRadius,
                    clickHandler, className);

    if (!text || !text[0]) return;

    const ImVec2 extent = ImGui::CalcTextSize(text);
    float offsetX = (width - extent.x) / 2.0f;
    float offsetY = (height - extent.y) / 2.0f;
    if (offsetX < 0.0f) offsetX = 0.0f;
    if (offsetY < 0.0f) offsetY = 0.0f;

    OnDrawText(x + offsetX, y + offsetY, text, fontSize, fontName, textColor, std::string(),
               std::string(), -1.0f, false);
}

}