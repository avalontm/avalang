#include "design/imgui_renderer.h"

#include <unordered_map>
#include <unordered_set>

#include "GLFW/glfw3.h"
#include "stb_image.h"

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

void ImGuiRenderer::OnDrawHtmlFragment(const std::string& html) {
    (void)html;
}

void ImGuiRenderer::OnDrawLink(float x, float y,
                               const char* text, float fontSize, const char* fontName,
                               const Color& color, const std::string& href,
                               const std::string& clickHandler,
                               const std::string& className) {

    (void)href;
    (void)clickHandler;
    (void)className;
    (void)fontName;
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
                                 const std::string& className) {
    (void)disabled;
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
