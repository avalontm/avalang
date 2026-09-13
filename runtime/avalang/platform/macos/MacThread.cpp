#include "MacThread.h"

#include <thread>
#include <chrono>
#include <cstdint>

namespace ava {
namespace platform {
namespace macos_ {

MacThread::MacThread(ThreadFunc func) : func_(std::move(func)) {
    thread_ = new std::thread([this]() {
        if (func_) func_();
    });
}

MacThread::~MacThread() {
    if (thread_) {
        if (thread_->joinable()) thread_->detach();
        delete thread_;
        thread_ = nullptr;
    }
}

void MacThread::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

bool MacThread::Joinable() const {
    return thread_ && thread_->joinable();
}

uint64_t MacThread::Id() const {
    if (!thread_) return 0;
    std::hash<std::thread::id> h;
    return static_cast<uint64_t>(h(thread_->get_id()));
}

IThread* MacThreadFactory::CreateThread(ThreadFunc func) {
    return new MacThread(std::move(func));
}

void MacThreadFactory::SleepMs(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

uint64_t MacThreadFactory::CurrentThreadId() const {
    std::hash<std::thread::id> h;
    return static_cast<uint64_t>(h(std::this_thread::get_id()));
}

} // namespace macos_
} // namespace platform
} // namespace ava
