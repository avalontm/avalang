#ifndef AVA_PLATFORM_PLATFORM_H
#define AVA_PLATFORM_PLATFORM_H

#include "interfaces/IPlatform.h"
#include "barekernel/stdcompat/ava_stdcompat.h"

#ifdef _WIN32
  #define AVA_PLATFORM_API __declspec(dllexport)
#else
  #define AVA_PLATFORM_API __attribute__((visibility("default")))
#endif

namespace ava {
namespace platform {

class AVA_PLATFORM_API Platform {
public:
    static avastd::unique_ptr<IPlatform> Create();
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_PLATFORM_H
