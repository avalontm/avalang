#ifndef AVA_PLATFORM_ITIMER_H
#define AVA_PLATFORM_ITIMER_H

// STABLE (Windows) since AVA_PAL_ABI_VERSION 1 -- see PAL_ABI.h.
// Fase 5 (Async Runtime, ver docs/PAL_PROGRESS.md). No confundir con
// platform::ui::ITimer (services/ui/ITimer.h): ese es el stub de Fase 0
// pensado para frame scheduling / animaciones de AvaUI (Fase 6). Este
// ITimer es de bajo nivel, usado por el runtime async de avalang.dll
// (sleep_async / await), no depende de UI ni de un event loop de ventana.
#include "PAL_ABI.h"

#include <cstdint>
#include <functional>

namespace ava {
namespace platform {

class ITimer {
public:
    virtual ~ITimer() = default;

    // Agenda callback para correr una vez tras delayMs, en un hilo del
    // pool del backend (NO garantiza que sea el hilo que llama). El
    // caller es responsable de sincronizar si toca estado compartido
    // (ver VmAsyncScheduler, que encola en vez de tocar la VM directo).
    virtual uint64_t ScheduleOnce(uint32_t delayMs, std::function<void()> callback) = 0;

    // Cancela un timer agendado. No-op si ya disparo o handle invalido.
    virtual void Cancel(uint64_t handle) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_ITIMER_H
