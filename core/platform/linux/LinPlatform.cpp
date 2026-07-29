#include "LinPlatform.h"
#include "LinMutex.h"
#include "../Platform.h"

namespace ava {
namespace platform {
namespace linux_ {

IMutex* LinPlatform::CreateMutex() {
    return new LinMutex();
}

} // namespace linux_

// Compile-time backend selection: this translation unit is only added to
// the build on Linux targets (see CMakeLists.txt). Windows builds add
// platform/windows/WinPlatform.cpp instead.
std::unique_ptr<IPlatform> Platform::Create() {
    return std::make_unique<linux_::LinPlatform>();
}

} // namespace platform
} // namespace ava
