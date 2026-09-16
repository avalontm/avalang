#include "WinInputState.h"
#include <windows.h>
#include <mutex>
#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

namespace {

std::mutex g_inputMutex;
float g_wheelDeltaX = 0.0f;
float g_wheelDeltaY = 0.0f;
std::vector<wchar_t> g_pendingChars;
std::vector<ava::platform::ui::NativeTouchPoint> g_touchPoints;
ava::platform::ui::ImeComposition g_imeComposition;

}

void WinInput_PushWheelDelta(float deltaX, float deltaY) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_wheelDeltaX += deltaX;
    g_wheelDeltaY += deltaY;
}

void WinInput_PushChar(unsigned int utf16CodeUnit) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_pendingChars.push_back(static_cast<wchar_t>(utf16CodeUnit));
}

void WinInput_SetTouchPoints(const std::vector<ava::platform::ui::NativeTouchPoint>& points) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_touchPoints = points;
}

void WinInput_SetImeComposition(bool active, const std::string& text, int cursor) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_imeComposition.active = active;
    g_imeComposition.text = text;
    g_imeComposition.cursor = cursor;
}

void WinWheel::ConsumeDelta(float& outDeltaX, float& outDeltaY) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    outDeltaX = g_wheelDeltaX;
    outDeltaY = g_wheelDeltaY;
    g_wheelDeltaX = 0.0f;
    g_wheelDeltaY = 0.0f;
}

std::string WinTextInput::ConsumeCommittedText() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    if (g_pendingChars.empty()) return {};

    int len = WideCharToMultiByte(CP_UTF8, 0, g_pendingChars.data(),
                                   static_cast<int>(g_pendingChars.size()),
                                   nullptr, 0, nullptr, nullptr);
    std::string result;
    if (len > 0) {
        result.resize(static_cast<size_t>(len));
        WideCharToMultiByte(CP_UTF8, 0, g_pendingChars.data(),
                             static_cast<int>(g_pendingChars.size()),
                             result.data(), len, nullptr, nullptr);
    }

    g_pendingChars.clear();
    return result;
}

std::vector<ava::platform::ui::NativeTouchPoint> WinTouch::ActivePoints() const {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return g_touchPoints;
}

ava::platform::ui::ImeComposition WinIme::CurrentComposition() const {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return g_imeComposition;
}

}
}
}
}
