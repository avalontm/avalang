#pragma once

#include <cstdint>
#include <string>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void Bridge_ShowKeyboard();
void Bridge_HideKeyboard();

void Bridge_QueryDisplayMetrics(int& width, int& height, float& density, int& densityDpi);

void Bridge_ClipboardSetText(const std::string& text);
std::string Bridge_ClipboardGetText();
bool Bridge_ClipboardHasText();

std::string Bridge_FilesDir();
std::string Bridge_CacheDir();

void Bridge_ShowNotification(const std::string& title, const std::string& body);
void Bridge_OpenUri(const std::string& uri);

void Bridge_RequestPermission(const std::string& permission, int requestCode);
bool Bridge_CheckPermission(const std::string& permission);

void Bridge_CanvasBeginFrame(int width, int height);
void Bridge_CanvasEndFrame();
void Bridge_CanvasDrawRect(float x, float y, float width, float height,
                            uint32_t fillColor, uint32_t borderColor, float borderWidth, float borderRadius);
void Bridge_CanvasDrawEllipse(float cx, float cy, float rx, float ry,
                               uint32_t fillColor, uint32_t borderColor, float borderWidth);
void Bridge_CanvasDrawText(float x, float y, const std::string& text,
                            float fontSize, const std::string& fontName, uint32_t color,
                            float maxWidth, bool wrap);
void Bridge_CanvasDrawImage(float x, float y, float width, float height, const std::string& imagePath);
void Bridge_CanvasDrawButton(float x, float y, float width, float height, const std::string& text,
                              float fontSize, const std::string& fontName,
                              uint32_t textColor, uint32_t fillColor, uint32_t borderColor,
                              float borderWidth, float borderRadius, bool disabled);
void Bridge_CanvasDrawLink(float x, float y, const std::string& text,
                            float fontSize, const std::string& fontName, uint32_t color);
void Bridge_CanvasPushClip(float x, float y, float width, float height);
void Bridge_CanvasPopClip();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
