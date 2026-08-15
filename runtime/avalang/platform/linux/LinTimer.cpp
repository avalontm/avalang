#include "LinTimer.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <cstdint>

namespace ava {
namespace platform {
namespace linux_ {

namespace {

struct PendingTimer {
    std::thread* thread = nullptr;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> done{false};
};

std::mutex g_registry_mtx;
std::unordered_map<uint64_t, PendingTimer*> g_registry;
std::atomic<uint64_t> g_next_id{1};

} // namespace

uint64_t LinTimer::ScheduleOnce(uint32_t delayMs, std::function<void()> callback) {
    if (!callback) return 0;
    auto* t = new PendingTimer();
    uint64_t id = g_next_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(g_registry_mtx);
        g_registry[id] = t;
    }
    t->thread = new std::thread([t, id, delayMs, cb = std::move(callback)]() {
        std::unique_lock<std::mutex> lk(t->mtx);
        if (t->cv.wait_for(lk, std::chrono::milliseconds(delayMs), [&] { return t->cancelled.load(); })) {
            // cancelled
        } else {
            lk.unlock();
            cb();
        }
        t->done.store(true);
        std::lock_guard<std::mutex> rlk(g_registry_mtx);
        g_registry.erase(id);
    });
    t->thread->detach();
    return id;
}

void LinTimer::Cancel(uint64_t handle) {
    std::lock_guard<std::mutex> lk(g_registry_mtx);
    auto it = g_registry.find(handle);
    if (it == g_registry.end()) return;
    PendingTimer* t = it->second;
    t->cancelled.store(true);
    t->cv.notify_all();
}

} // namespace linux_
} // namespace platform
} // namespace ava
