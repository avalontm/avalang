#ifndef AVA_PLATFORM_LIN_THREAD_H
#define AVA_PLATFORM_LIN_THREAD_H

#include "../interfaces/IThread.h"
#include <thread>

namespace ava {
namespace platform {
namespace linux_ {

// STUB. TODO: back with pthread_create/pthread_join (Linux) or the
// equivalent Darwin pthread APIs (macOS).
class LinThread : public IThread {
public:
    explicit LinThread(ThreadFunc func);
    ~LinThread() override;

    void Join() override;
    bool Joinable() const override;
    uint64_t Id() const override;

private:
    ThreadFunc func_;
    std::thread* thread_ = nullptr;
};

class LinThreadFactory : public IThreadFactory {
public:
    IThread* CreateThread(ThreadFunc func) override;
    void SleepMs(uint32_t milliseconds) override;
    uint64_t CurrentThreadId() const override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_THREAD_H
