#include "WinThread.h"

namespace ava {
namespace platform {
namespace windows {

WinThread::WinThread(ThreadFunc func) : func_(std::move(func)) {
    handle_ = CreateThread(nullptr, 0, &WinThread::ThreadTrampoline, this, 0, &thread_id_);
}

WinThread::~WinThread() {
    if (handle_ != nullptr) {
        if (!joined_) {
            Join();
        }
        CloseHandle(handle_);
    }
}

DWORD WINAPI WinThread::ThreadTrampoline(LPVOID param) {
    WinThread* self = static_cast<WinThread*>(param);
    if (self->func_) {
        self->func_();
    }
    return 0;
}

void WinThread::Join() {
    if (handle_ != nullptr && !joined_) {
        WaitForSingleObject(handle_, INFINITE);
        joined_ = true;
    }
}

bool WinThread::Joinable() const {
    return handle_ != nullptr && !joined_;
}

uint64_t WinThread::Id() const {
    return static_cast<uint64_t>(thread_id_);
}

IThread* WinThreadFactory::CreateThread(ThreadFunc func) {
    return new WinThread(std::move(func));
}

void WinThreadFactory::SleepMs(uint32_t milliseconds) {
    ::Sleep(static_cast<DWORD>(milliseconds));
}

uint64_t WinThreadFactory::CurrentThreadId() const {
    return static_cast<uint64_t>(::GetCurrentThreadId());
}

} // namespace windows
} // namespace platform
} // namespace ava
