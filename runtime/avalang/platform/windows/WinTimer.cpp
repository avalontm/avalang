#include "WinTimer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>

namespace ava {
namespace platform {
namespace windows {

int64_t WinTimer::NowMs() {
    return static_cast<int64_t>(GetTickCount64());
}

WinTimer::WinTimer() {
    worker_ = std::thread(&WinTimer::WorkerLoop, this);
}

WinTimer::~WinTimer() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutting_down_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

uint64_t WinTimer::ScheduleOnce(uint32_t delayMs, std::function<void()> callback) {
    auto task = std::make_shared<Task>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        task->handle = next_handle_++;
        task->due_ms = NowMs() + static_cast<int64_t>(delayMs);
        task->callback = std::move(callback);
        tasks_.push_back(task);
        by_handle_[task->handle] = task;
    }
    cv_.notify_all();
    return task->handle;
}

void WinTimer::Cancel(uint64_t handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_handle_.find(handle);
    if (it == by_handle_.end()) return;
    it->second->cancelled = true;
    by_handle_.erase(it);
    // El WorkerLoop limpia entradas canceladas de tasks_ en su proxima
    // pasada; no hace falta notificar, el peor caso es que espere hasta
    // el proximo vencimiento (no hay busy-wait).
}

void WinTimer::WorkerLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!shutting_down_) {
        // Purga cancelados / ya disparados.
        tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                     [](const std::shared_ptr<Task>& t) { return t->cancelled; }),
                     tasks_.end());

        if (tasks_.empty()) {
            cv_.wait(lock, [this] { return shutting_down_.load() || !tasks_.empty(); });
            continue;
        }

        auto next = std::min_element(tasks_.begin(), tasks_.end(),
                                      [](const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
                                          return a->due_ms < b->due_ms;
                                      });

        int64_t wait_ms = (*next)->due_ms - NowMs();
        if (wait_ms > 0) {
            cv_.wait_for(lock, std::chrono::milliseconds(wait_ms),
                         [this] { return shutting_down_.load(); });
            continue; // reevalua: pudo haberse agendado algo mas urgente o cancelado esto
        }

        // Vencio: lo sacamos, disparamos fuera del lock.
        auto task = *next;
        tasks_.erase(next);
        by_handle_.erase(task->handle);
        if (task->cancelled) continue;

        lock.unlock();
        if (task->callback) task->callback();
        lock.lock();
    }
}

} // namespace windows
} // namespace platform
} // namespace ava
