#ifndef AVA_VM_PLATFORM_ACCESSOR_H
#define AVA_VM_PLATFORM_ACCESSOR_H

#include "../platform/interfaces/IPlatform.h"
#include <memory>

#ifdef _WIN32
  #define AVA_VM_PLATFORM_ACCESSOR_API __declspec(dllexport)
#else
  #define AVA_VM_PLATFORM_ACCESSOR_API __attribute__((visibility("default")))
#endif

namespace ava {

// Thread-safe singleton accessor to the Platform Abstraction Layer.
// Provides VM, Runtime, and Compiler with a single entry point to OS services.
class AVA_VM_PLATFORM_ACCESSOR_API VmPlatformAccessor {
public:
    // Returns the active IPlatform instance (lazy-initialized, thread-safe).
    // If an override is installed (see SetOverride below), returns that
    // instead of the real platform::Platform::Create() backend.
    static platform::IPlatform& Get();

    // Fase 7 (avapack, filesystem virtual en memoria): permite reemplazar
    // el IPlatform devuelto por Get() con uno propio (tipicamente
    // MemoryOverridePlatform, ver platform/memory/MemoryOverridePlatform.h)
    // ANTES de crear el primer AvaVM -- ModuleResolver/ModuleCache y
    // cualquier otro consumidor de VmPlatformAccessor::Get() lo ven desde
    // la primera llamada. Exportado (AVA_VM_PLATFORM_ACCESSOR_API) porque
    // el .exe empacado (runtime/avapack/src/main_zerodisk.cpp) lo llama
    // desde fuera de avalang.dll, mismo patron que Platform::Create()
    // (AVA_PLATFORM_API, ver Fase 2). ava_cli/avahost normales nunca
    // llaman esto y siguen usando el IPlatform real sin cambios.
    static void SetOverride(std::unique_ptr<platform::IPlatform> platform);
    static void ClearOverride();

private:
    VmPlatformAccessor() = delete;
    ~VmPlatformAccessor() = delete;
};

} // namespace ava

#endif // AVA_VM_PLATFORM_ACCESSOR_H
