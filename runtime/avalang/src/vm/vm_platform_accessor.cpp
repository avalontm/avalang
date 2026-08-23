#include "vm_platform_accessor.h"
#include "../platform/Platform.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

namespace {
avastd::mutex g_override_mutex;
avastd::unique_ptr<platform::IPlatform> g_override_platform;

// Reemplaza al Meyer's Singleton original (`static avastd::unique_ptr<IPlatform>
// g_real = Platform::Create();` dentro de Get()) por dos globales de
// namespace, triviales (bool zero-init + unique_ptr default-construido a
// nullptr, ninguno necesita entrada en .init_array) con un `if` explicito
// escrito a mano -- evita el codegen implicito de guard variable de una
// static local con inicializador no trivial.
platform::IPlatform* g_real_platform_ptr = nullptr;
bool g_real_platform_initialized = false;
} // namespace

platform::IPlatform& VmPlatformAccessor::Get() {
    if (!g_real_platform_initialized) {
        // Ownership: se guarda en un unique_ptr local solo para que
        // Platform::Create() (que devuelve unique_ptr<IPlatform>) se
        // pueda mover comodo; el puntero crudo se extrae con release()
        // y se guarda en g_real_platform_ptr, que vive el resto del
        // proceso (mismo tiempo de vida que tenia el Meyer's singleton).
        avastd::unique_ptr<platform::IPlatform> created = platform::Platform::Create();
        g_real_platform_ptr = created.release();
        g_real_platform_initialized = true;
    }

    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);

    if (g_override_platform) return *g_override_platform;

    return *g_real_platform_ptr;
}

void VmPlatformAccessor::SetOverride(avastd::unique_ptr<platform::IPlatform> platform) {
    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);
    g_override_platform = avastd::move(platform);
}

void VmPlatformAccessor::ClearOverride() {
    avastd::lock_guard<avastd::mutex> lock(g_override_mutex);
    g_override_platform.reset();
}

} // namespace ava
