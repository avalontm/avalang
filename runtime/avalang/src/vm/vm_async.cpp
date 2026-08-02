// Fase 5 (Async Runtime). Ver docs/PAL_PROGRESS.md.
//
// Este archivo implementa la parte de la VM que consume ITimer para dar
// soporte a "delayed execution" (set_timeout), el primer bloque de la
// lista del Development Strategy / async doc bullet list en
// docs/Platform_Foundation.md ("async/await, animations, dispatcher,
// delayed execution, frame scheduling"). await/animations/frame
// scheduling sobre coroutines quedan para una sub-fase siguiente (ver
// nota en PAL_PROGRESS.md) -- este dispatcher (callback-based, estilo
// setTimeout) ya es suficiente para desacoplar scripts AvaLang de
// Sleep() bloqueante usando el PAL en vez de winapi directo.
#include "vm.h"

namespace ava {

void VM::PostAsyncTask(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(async_mutex_);
    async_ready_queue_.push_back(std::move(task));
}

void VM::PumpAsyncEvents() {
    std::vector<std::function<void()>> ready;
    {
        std::lock_guard<std::mutex> lock(async_mutex_);
        ready.swap(async_ready_queue_);
    }
    for (auto& task : ready) {
        if (task) task();
    }
}

bool VM::HasPendingAsyncWork() const {
    if (async_pending_timers_.load() > 0) return true;
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(async_mutex_));
    return !async_ready_queue_.empty();
}

void VM::OnAsyncTimerScheduled() {
    async_pending_timers_.fetch_add(1);
}

void VM::OnAsyncTimerConsumed() {
    async_pending_timers_.fetch_sub(1);
}

} // namespace ava
