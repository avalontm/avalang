#include "AndroidPlatform.h"
#include "AndroidAppSurface.h"
#include "AndroidBackendEntry.h"

namespace avalang {
namespace ui {
namespace platform {

AppSurface* android::AndroidPlatform::CreateSurface() {
    return new android::AndroidAppSurface();
}

namespace android {

namespace {
AndroidPlatform g_instance;

struct PlatformRegistrar {
    PlatformRegistrar() {
        IPlatform::SetInstance(&g_instance);
    }
};

const PlatformRegistrar g_registrar;
}

void LinkBackend() {}

}

}
}
}
