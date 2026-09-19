#ifndef AVA_STDCOMPAT_CONDITION_VARIABLE_H
#define AVA_STDCOMPAT_CONDITION_VARIABLE_H

#include "ava_platform_caps.h"
#include "ava_mutex.h"

#if AVA_HAVE_STD_LIBRARY

#include <condition_variable>
#include <mutex>
namespace avastd {
using condition_variable = std::condition_variable;
template <class M> using unique_lock = std::unique_lock<M>;
}

#else

namespace avastd {

template <class M>
class unique_lock {
public:
    explicit unique_lock(M& m) noexcept : m_(&m), owns_(true) { m_->lock(); }
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;
    ~unique_lock() { if (owns_) m_->unlock(); }

    void lock() noexcept { m_->lock(); owns_ = true; }
    void unlock() noexcept { m_->unlock(); owns_ = false; }
    M* mutex() const noexcept { return m_; }

private:
    M* m_;
    bool owns_;
};

class condition_variable {
public:
    condition_variable() noexcept = default;
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    template <class M>
    void wait(unique_lock<M>&) noexcept {}

    template <class M, class Pred>
    void wait(unique_lock<M>&, Pred) noexcept {}

    void notify_one() noexcept {}
    void notify_all() noexcept {}
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_CONDITION_VARIABLE_H
