#include "IPlatform.h"

#include <stdexcept>

namespace avalang {
namespace ui {
namespace platform {

namespace {
IPlatform* g_instance = nullptr;
}

void IPlatform::SetInstance(IPlatform* instance) {
    g_instance = instance;
}

IPlatform& IPlatform::Current() {
    if (!g_instance) {
        throw std::runtime_error(
            "avalang.ui: no platform backend registered. Link the DLL/lib for "
            "your target platform (avalang_ui_win, avalang_ui_linux, "
            "avalang_ui_macos, avalang_ui_android) and make sure it gets "
            "loaded before calling into avalang.ui.");
    }
    return *g_instance;
}

IPlatform& GetPlatform() {
    return IPlatform::Current();
}

}
}
}
