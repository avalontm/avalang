#ifndef AVA_PLATFORM_MAC_THREAD_H
#define AVA_PLATFORM_MAC_THREAD_H

#include "../interfaces/IThread.h"
#include <thread>

namespace ava {
namespace platform {
namespace macos_ {

class MacThread : public IThread {
public:
    explicit MacThread(ThreadFunc func);
    ~MacThread() override;

    void Join() override;
    bool Joinable() const override;
    uint64_t Id() const override;

private:
    ThreadFunc func_;
    std::thread* thread_ = nullptr;
};

class MacThreadFactory : public IThreadFactory {
public:
    IThread* CreateThread(ThreadFunc func) override;
    void SleepMs(uint32_t milliseconds) override;
    uint64_t CurrentThreadId() const override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_THREAD_H
