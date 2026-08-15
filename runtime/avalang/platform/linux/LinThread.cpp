#include "LinThread.h"

#include <thread>
#include <chrono>
#include <cstdint>

namespace ava {
namespace platform {
namespace linux_ {

LinThread::LinThread(ThreadFunc func) : func_(std::move(func)) {
    thread_ = new std::thread([this]() {
        if (func_) func_();
    });
}

LinThread::~LinThread() {
    if (thread_) {
        if (thread_->joinable()) thread_->detach();
        delete thread_;
        thread_ = nullptr;
    }
}

void LinThread::Join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

bool LinThread::Joinable() const {
    return thread_ && thread_->joinable();
}

uint64_t LinThread::Id() const {
    if (!thread_) return 0;
    std::hash<std::thread::id> h;
    return static_cast<uint64_t>(h(thread_->get_id()));
}

IThread* LinThreadFactory::CreateThread(ThreadFunc func) {
    return new LinThread(std::move(func));
}

void LinThreadFactory::SleepMs(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

uint64_t LinThreadFactory::CurrentThreadId() const {
    std::hash<std::thread::id> h;
    return static_cast<uint64_t>(h(std::this_thread::get_id()));
}

} // namespace linux_
} // namespace platform
} // namespace ava
