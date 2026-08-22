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
    return avastd::make_unique<barekernel::BareKernelPlatform>();
}

} // namespace platform
} // namespace ava
