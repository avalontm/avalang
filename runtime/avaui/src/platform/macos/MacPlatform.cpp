#include "MacPlatform.h"
#include "MacBackendEntry.h"

namespace avalang {
namespace ui {
namespace platform {

AppSurface* MacPlatform::CreateSurface() {
    return new stub::StubAppSurface();
}

namespace {
MacPlatform g_instance;

struct PlatformRegistrar {
    PlatformRegistrar() {
        IPlatform::SetInstance(&g_instance);
    }
};

const PlatformRegistrar g_registrar;
}

void LinkMacBackend() {}

}
}
}