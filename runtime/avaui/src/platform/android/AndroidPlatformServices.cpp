#include "AndroidPlatformServices.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

bool AndroidPlatformServices::OpenFileDialog(std::string&) {
    return false;
}

bool AndroidPlatformServices::SaveFileDialog(std::string&) {
    return false;
}

void AndroidPlatformServices::ShowNotification(const std::string& title, const std::string& body) {
    Bridge_ShowNotification(title, body);
}

void AndroidPlatformServices::OpenUri(const std::string& uri) {
    Bridge_OpenUri(uri);
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
