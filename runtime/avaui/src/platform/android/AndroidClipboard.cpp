#include "AndroidClipboard.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidClipboard::SetText(const std::string& text) {
    Bridge_ClipboardSetText(text);
}

std::string AndroidClipboard::GetText() const {
    return Bridge_ClipboardGetText();
}

bool AndroidClipboard::HasText() const {
    return Bridge_ClipboardHasText();
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
