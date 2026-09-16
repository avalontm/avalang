#include "IosPlatform.h"
#include "IosBackendEntry.h"

namespace avalang {
namespace ui {
namespace platform {

AppSurface* IosPlatform::CreateSurface() {
    return new stub::StubAppSurface();
}

namespace {
IosPlatform g_instance;

struct PlatformRegistrar {
    PlatformRegistrar() {
        IPlatform::SetInstance(&g_instance);
    }
};

const PlatformRegistrar g_registrar;
}

void LinkIosBackend() {}

}
}
}
