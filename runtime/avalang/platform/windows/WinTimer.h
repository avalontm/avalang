#ifndef AVA_PLATFORM_WIN_TIMER_H
#define AVA_PLATFORM_WIN_TIMER_H

#include "../interfaces/ITimer.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ava {
namespace platform {
namespace windows {

// Implementacion simple de ITimer: un unico hilo trabajador duerme hasta
// el proximo vencimiento (lista ordenada por deadline) y dispara
// callbacks en ese hilo, uno a la vez. Deliberadamente NO se usa el
// Win32 threadpool (CreateThreadpoolTimer) para evitar la gestion de
// cierre del timer desde dentro de su propio callback -- este approach
// es mas facil de razonar y suficiente para el volumen de timers que un
// script AvaLang puede agendar (sleep_async / futuros setInterval).
class WinTimer : public ITimer {
public:
    WinTimer();
    ~WinTimer() override;

    uint64_t ScheduleOnce(uint32_t delayMs, std::function<void()> callback) override;
    void Cancel(uint64_t handle) override;

private:
    struct Task {
        uint64_t handle = 0;
        int64_t due_ms = 0; // reloj monotonico
        std::function<void()> callback;
        bool cancelled = false;
    };

    void WorkerLoop();
    static int64_t NowMs();

    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<Task>> tasks_; // volumen bajo: orden lineal, no heap real
    std::unordered_map<uint64_t, std::shared_ptr<Task>> by_handle_;
    uint64_t next_handle_ = 1;
    std::atomic<bool> shutting_down_{false};
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_TIMER_H
