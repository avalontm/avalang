#include "WinPlatform.h"
#include "WinMutex.h"
#include "../Platform.h"

namespace ava {
namespace platform {
namespace windows {

IMutex* WinPlatform::CreateMutex() {
    return new WinMutex();
}

} // namespace windows

// Compile-time backend selection: this translation unit is only added to
// the build on Windows targets, so Platform::Create() resolves here.
// Linux/macOS builds add their own PlatformFactory .cpp under
// platform/linux/ and platform/macos/ instead (Phase 5 / Phase 6).
std::unique_ptr<IPlatform> Platform::Create() {
    return std::make_unique<windows::WinPlatform>();
}

} // namespace platform
} // namespace ava
