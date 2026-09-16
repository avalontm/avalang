#include "AndroidPlatformPaths.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

std::string AndroidPlatformPaths::FontsDir() const {
    return "/system/fonts";
}

std::string AndroidPlatformPaths::SystemDir() const {
    return "/system";
}

std::string AndroidPlatformPaths::AppDataDir() const {
    return Bridge_FilesDir();
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
