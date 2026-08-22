#ifndef AVA_STDCOMPAT_MUTEX_H
#define AVA_STDCOMPAT_MUTEX_H

#include "ava_platform_caps.h"

// avastd::mutex delega en las mismas syscalls CKM que usa BareKernelMutex
// cuando AVA_HAVE_MUTEX (CKM_CAP_MUTEX) esta activo, en vez del no-op que
// tenia antes. No pasa por la interfaz polimorfica IMutex/IPlatform a
// proposito: avastd es la capa de mas bajo nivel y el VM la usa como tipo
// de valor (async_mutex_), no como puntero a interfaz. Con
// CKM_CAP_MUTEX=0 (este kernel hoy) cae al no-op de siempre, que sigue
// siendo observacionalmente correcto porque CKM_CAP_THREADS=0 tambien
// implica que no hay ejecucion paralela real dentro de un mismo proceso.

#if AVA_HAVE_STD_LIBRARY

#include <mutex>
namespace avastd {
using mutex = std::mutex;
template <class M> using lock_guard = std::lock_guard<M>;
}

#else

#if AVA_HAVE_MUTEX
#include "../ckm_contract.h"
#endif

namespace avastd {

class mutex {
public:
#if AVA_HAVE_MUTEX
    // Mismas syscalls que BareKernelMutex (CKM_CAP_MUTEX branch, ver
    // BareKernelMutex.cpp) llamadas directo sobre un CkmMutex propio, sin
    // pasar por la interfaz polimorfica IMutex/IPlatform: avastd es una
    // capa por debajo de eso, y VM ya usa avastd::mutex como tipo de valor
    // (async_mutex_), no como puntero a interfaz.
    mutex() noexcept { ckm_mutex_init(&raw_); }
#else
    mutex() noexcept = default;
#endif
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

#if AVA_HAVE_MUTEX
    void lock() noexcept { ckm_mutex_lock(&raw_); }
    void unlock() noexcept { ckm_mutex_unlock(&raw_); }
    bool try_lock() noexcept { return ckm_mutex_trylock(&raw_) == 0; }
private:
    CkmMutex raw_;
#else
    void lock() noexcept {}
    void unlock() noexcept {}
    bool try_lock() noexcept { return true; }
#endif
};

template <class M>
class lock_guard {
public:
    explicit lock_guard(M& m) noexcept : m_(m) { m_.lock(); }
    ~lock_guard() { m_.unlock(); }
    lock_guard(const lock_guard&) = delete;
private:
    M& m_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_MUTEX_H
