#include "LinPlatform.h"
#include "LinBackendEntry.h"

namespace avalang {
namespace ui {
namespace platform {

AppSurface* LinPlatform::CreateSurface() {
    return new stub::StubAppSurface();
}

namespace {
LinPlatform g_instance;

struct PlatformRegistrar {
    PlatformRegistrar() {
        IPlatform::SetInstance(&g_instance);
    }
};

const PlatformRegistrar g_registrar;
}

void LinkLinuxBackend() {}

}
}
}