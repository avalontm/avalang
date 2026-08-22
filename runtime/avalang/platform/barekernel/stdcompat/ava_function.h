#ifndef AVA_STDCOMPAT_FUNCTION_H
#define AVA_STDCOMPAT_FUNCTION_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_utility.h"
#include "ava_new.h"

#if AVA_HAVE_STD_LIBRARY

#include <functional>
namespace avastd {
template <class Sig> using function = std::function<Sig>;
}

#else

namespace avastd {

// Type erasure clasico (concept/model), sin small-buffer-optimization:
// siempre un allocation en heap para el callable envuelto. Cubre lambdas
// con captura, punteros a funcion, y functores -- que es todo lo que
// AvaLang instancia hoy (callbacks de PostAsyncTask/PrintSink/InputSink/
// etc. en vm.h). No soporta target(), ni comparacion con nullptr_t via
// operator==, porque AvaLang no los usa; se agregan si una fase futura los
// necesita.
template <class Sig> class function;

template <class R, class... Args>
class function<R(Args...)> {
    struct CallableBase {
        virtual ~CallableBase() = default;
        virtual R invoke(Args... args) = 0;
        virtual CallableBase* clone() const = 0;
    };
    template <class F>
    struct CallableModel : CallableBase {
        F f;
        explicit CallableModel(F fn) : f(avastd::move(fn)) {}
        R invoke(Args... args) override { return f(avastd::forward<Args>(args)...); }
        CallableBase* clone() const override { return new CallableModel<F>(f); }
    };

public:
    function() noexcept : callable_(nullptr) {}
    function(avastd::nullptr_t) noexcept : callable_(nullptr) {}

    template <class F>
    function(F f) : callable_(new CallableModel<F>(avastd::move(f))) {}

    function(const function& other) : callable_(other.callable_ ? other.callable_->clone() : nullptr) {}
    function(function&& other) noexcept : callable_(other.callable_) { other.callable_ = nullptr; }

    ~function() { delete callable_; }

    function& operator=(const function& other) {
        if (this == &other) return *this;
        delete callable_;
        callable_ = other.callable_ ? other.callable_->clone() : nullptr;
        return *this;
    }
    function& operator=(function&& other) noexcept {
        if (this == &other) return *this;
        delete callable_;
        callable_ = other.callable_;
        other.callable_ = nullptr;
        return *this;
    }
    function& operator=(avastd::nullptr_t) noexcept { delete callable_; callable_ = nullptr; return *this; }

    R operator()(Args... args) const { return callable_->invoke(avastd::forward<Args>(args)...); }

    explicit operator bool() const noexcept { return callable_ != nullptr; }

private:
    CallableBase* callable_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_FUNCTION_H
