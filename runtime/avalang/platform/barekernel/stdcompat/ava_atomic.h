#ifndef AVA_STDCOMPAT_ATOMIC_H
#define AVA_STDCOMPAT_ATOMIC_H

#include "ava_platform_caps.h"

// <atomic> tambien vive en libstdc++ (es una libreria, no solo plantillas:
// necesita saber que instrucciones de CPU usar). Sin ella, nos apoyamos en
// los builtins __atomic_* que el COMPILADOR provee directo (no requieren
// libstdc++, son intrinsics de GCC/Clang que bajan a instrucciones lock/
// xadd/cmpxchg de x86). AvaLang solo usa std::atomic<int64_t> como contador
// de referencias (ver Object::ref_count en value.h) y operaciones basicas
// (load/store/fetch_add/fetch_sub/compare_exchange) -- esta es la
// superficie que se cubre, no el std::atomic completo.
//
// Nota sobre CKM_CAP_THREADS=0: sin hilos reales en este kernel, estas
// operaciones no necesitan ser lock-free multi-nucleo, pero se implementan
// con los builtins de todos modos (son gratis y correctas tambien en
// single-core, y dejan el codigo listo para cuando CKM_CAP_THREADS pase a 1).

#if AVA_HAVE_STD_LIBRARY

#include <atomic>
namespace avastd {
template <class T> using atomic = std::atomic<T>;
using memory_order = std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;
}

#else

namespace avastd {

// memory_order: AvaLang solo usa relaxed/acquire/release/acq_rel (ver
// Object::ref_count en value.h y BareKernelMutex.cpp) -- no seq_cst
// explicito en ningun lado hoy, pero se agrega igual por completitud y
// porque no cuesta nada (son solo constantes que se mapean 1:1 a los
// __ATOMIC_* builtins de abajo).
enum class memory_order {
    relaxed = __ATOMIC_RELAXED,
    consume = __ATOMIC_CONSUME,
    acquire = __ATOMIC_ACQUIRE,
    release = __ATOMIC_RELEASE,
    acq_rel = __ATOMIC_ACQ_REL,
    seq_cst = __ATOMIC_SEQ_CST,
};
constexpr memory_order memory_order_relaxed = memory_order::relaxed;
constexpr memory_order memory_order_consume = memory_order::consume;
constexpr memory_order memory_order_acquire = memory_order::acquire;
constexpr memory_order memory_order_release = memory_order::release;
constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

template <class T>
class atomic {
public:
    atomic() noexcept : value_(T()) {}
    constexpr atomic(T desired) noexcept : value_(desired) {}

    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;

    T load(memory_order order = memory_order::seq_cst) const noexcept {
        return __atomic_load_n(&value_, static_cast<int>(order));
    }
    void store(T desired, memory_order order = memory_order::seq_cst) noexcept {
        __atomic_store_n(&value_, desired, static_cast<int>(order));
    }
    T fetch_add(T arg, memory_order order = memory_order::seq_cst) noexcept {
        return __atomic_fetch_add(&value_, arg, static_cast<int>(order));
    }
    T fetch_sub(T arg, memory_order order = memory_order::seq_cst) noexcept {
        return __atomic_fetch_sub(&value_, arg, static_cast<int>(order));
    }
    bool compare_exchange_strong(T& expected, T desired,
                                  memory_order success = memory_order::seq_cst,
                                  memory_order failure = memory_order::seq_cst) noexcept {
        return __atomic_compare_exchange_n(&value_, &expected, desired, false,
                                            static_cast<int>(success), static_cast<int>(failure));
    }

    operator T() const noexcept { return load(); }
    T operator=(T desired) noexcept { store(desired); return desired; }
    T operator++() noexcept { return fetch_add(1) + 1; }
    T operator++(int) noexcept { return fetch_add(1); }
    T operator--() noexcept { return fetch_sub(1) - 1; }
    T operator--(int) noexcept { return fetch_sub(1); }

private:
    T value_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_ATOMIC_H
