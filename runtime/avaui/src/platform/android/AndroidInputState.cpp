#include "AndroidInputState.h"

#include <mutex>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

std::mutex g_inputMutex;
std::vector<ava::platform::ui::NativeTouchPoint> g_touchPoints;
std::string g_pendingText;
ava::platform::ui::ImeComposition g_imeComposition;

}

void AndroidInput_SetTouchPoints(const std::vector<ava::platform::ui::NativeTouchPoint>& points) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_touchPoints = points;
}

void AndroidInput_PushCommittedText(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_pendingText += text;
}

void AndroidInput_SetImeComposition(bool active, const std::string& text, int cursor) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_imeComposition.active = active;
    g_imeComposition.text = text;
    g_imeComposition.cursor = cursor;
}

std::string AndroidTextInput::ConsumeCommittedText() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    std::string result = g_pendingText;
    g_pendingText.clear();
    return result;
}

std::vector<ava::platform::ui::NativeTouchPoint> AndroidTouch::ActivePoints() const {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return g_touchPoints;
}

ava::platform::ui::ImeComposition AndroidIme::CurrentComposition() const {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return g_imeComposition;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
