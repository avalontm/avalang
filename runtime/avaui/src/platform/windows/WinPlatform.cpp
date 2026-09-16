#include "WinPlatform.h"
#include "WinAppSurface.h"
#include "WinBackendEntry.h"

namespace avalang {
namespace ui {
namespace platform {

AppSurface* WinPlatform::CreateSurface() {
    return new windows::WinAppSurface();
}

namespace windows {

namespace {
WinPlatform g_instance;

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