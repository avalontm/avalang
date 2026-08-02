#ifndef AVA_PLATFORM_ITHREAD_H
#define AVA_PLATFORM_ITHREAD_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h for the
// freeze/deprecation policy before changing any signature in IThread / IThreadFactory.
#include "PAL_ABI.h"

#include <functional>
#include <cstdint>

namespace ava {
namespace platform {

using ThreadFunc = std::function<void()>;

// A single OS thread handle.
class IThread {
public:
    virtual ~IThread() = default;

    virtual void Join() = 0;
    virtual bool Joinable() const = 0;
    virtual uint64_t Id() const = 0;
};

// Factory + static thread utilities. One instance per process, obtained via
// the platform factory (see core/platform/Platform.h, added when the
// concrete backends land).
class IThreadFactory {
public:
    virtual ~IThreadFactory() = default;

    virtual IThread* CreateThread(ThreadFunc func) = 0;
    virtual void SleepMs(uint32_t milliseconds) = 0;
    virtual uint64_t CurrentThreadId() const = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_ITHREAD_H
