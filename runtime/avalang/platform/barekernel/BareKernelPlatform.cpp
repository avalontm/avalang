#include "BareKernelPlatform.h"
#include "BareKernelMutex.h"
#include "../Platform.h"

namespace ava {
namespace platform {
namespace barekernel {

IMutex* BareKernelPlatform::CreateMutex() {
    return new BareKernelMutex();
}

} // namespace barekernel

// Compile-time backend selection: this translation unit is only added to
// the build when AVA_TARGET_BAREKERNEL is defined, so Platform::Create()
// resolves here instead of the Windows/Linux/macOS backend.
avastd::unique_ptr<IPlatform> Platform::Create() {
    avastd::unique_ptr<barekernel::BareKernelPlatform> concrete =
        avastd::make_unique<barekernel::BareKernelPlatform>();
    return avastd::move(concrete);
}

} // namespace platform
} // namespace ava
