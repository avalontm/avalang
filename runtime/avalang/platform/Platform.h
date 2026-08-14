#ifndef AVA_PLATFORM_PLATFORM_H
#define AVA_PLATFORM_PLATFORM_H

#include "interfaces/IPlatform.h"
#include <memory>

#ifdef _WIN32
  #define AVA_PLATFORM_API __declspec(dllexport)
#else
  #define AVA_PLATFORM_API __attribute__((visibility("default")))
#endif

namespace ava {
namespace platform {

// Single entry point for Runtime/VM/Compiler/UI to obtain the active
// platform backend. The concrete implementation is chosen at compile time
// (see below) -- there is no runtime OS detection.
//
// Exportado (AVA_PLATFORM_API) desde Fase 2 de avapack: antes solo lo usaba
// código interno de avalang (ver vm_platform_accessor.cpp), pero
// runtime/avacli/src/build_command.cpp (el subcomando `ava_cli build`)
// necesita llamarlo desde fuera del .dll para usar IProcess y disparar
// `cmake` sin pasar por system() crudo. Mismo patrón que AVA_MODULE_API /
// AVA_VM_API en runtime/avalang/src/vm/.
class AVA_PLATFORM_API Platform {
public:
    static std::unique_ptr<IPlatform> Create();
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_PLATFORM_H
