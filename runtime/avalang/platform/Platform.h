#ifndef AVA_PLATFORM_PLATFORM_H
#define AVA_PLATFORM_PLATFORM_H

#include "interfaces/IPlatform.h"
#include <memory>

namespace ava {
namespace platform {

// Single entry point for Runtime/VM/Compiler/UI to obtain the active
// platform backend. The concrete implementation is chosen at compile time
// (see below) -- there is no runtime OS detection.
class Platform {
public:
    static std::unique_ptr<IPlatform> Create();
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_PLATFORM_H
