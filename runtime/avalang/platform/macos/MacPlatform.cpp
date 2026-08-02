#include "MacPlatform.h"
#include "MacMutex.h"
#include "../Platform.h"

namespace ava {
namespace platform {
namespace macos_ {

IMutex* MacPlatform::CreateMutex() {
    return new MacMutex();
}

} // namespace macos_

// Compile-time backend selection: this translation unit is only added to
// the build on macOS targets (see CMakeLists.txt). Windows builds add
// platform/windows/WinPlatform.cpp instead.
std::unique_ptr<IPlatform> Platform::Create() {
    return std::make_unique<macos_::MacPlatform>();
}

} // namespace platform
} // namespace ava
