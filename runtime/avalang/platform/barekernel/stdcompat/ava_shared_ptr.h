#ifndef AVA_STDCOMPAT_SHARED_PTR_H
#define AVA_STDCOMPAT_SHARED_PTR_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_utility.h"
#include "ava_atomic.h"
#include "ava_new.h"

#if AVA_HAVE_STD_LIBRARY

#include <memory>
namespace avastd {
template <class T> using shared_ptr = std::shared_ptr<T>;
template <class T> using weak_ptr = std::weak_ptr<T>;
template <class T, class D = std::default_delete<T>> using unique_ptr = std::unique_ptr<T, D>;
using std::make_shared;
using std::make_unique;
}

#else

namespace avastd {

// Control block con conteo atomico (reusa avastd::atomic -- ver ava_atomic.h)
// separado del objeto (no intrusivo), igual que std::shared_ptr con
// make_shared salvo que no fusiona la alocacion en un solo bloque (esa
// optimizacion se puede agregar despues sin romper la API si hace falta).
//
// `destroy_` es un puntero a funcion type-erased (void*, no T*) capturado
// en el CONSTRUCTOR -- igual que hace std::shared_ptr real. Es lo que
// permite que shared_ptr<T> funcione con T solo forward-declarado en el
// punto donde el shared_ptr se DESTRUYE (ej. BoundMethod::proto en
// value.h, con Proto solo forward-declarado ahi): el `delete ptr` real,
// tipado, queda encerrado en DefaultDelete<T>, que solo se instancia (y
// por lo tanto solo exige T completo) en el sitio donde alguien construye
// el shared_ptr desde un T* real -- ahi T ya es necesariamente completo
// porque hizo falta para crear el objeto. Antes `release()` tenia
// `delete ptr` directo tipado en T, lo que exigia T completo en TODOS los
// TU que instancian ~shared_ptr<T>() (cualquiera que destruya un
// BoundMethod), y eso no es cierto en general -- encontrado via el
// syntax-check de Fase 6 (ver §11 del audit).
template <class T>
struct ControlBlock {
    T* ptr;
    atomic<long> ref_count;
    void (*deleter)(T*);        // custom deleter, nullptr = usar default_destroy_
    void (*default_destroy_)(void*);  // type-erased `delete ptr`, ver nota arriba
    template <class U>
    static void DefaultDelete(void* p) { delete static_cast<U*>(p); }
    explicit ControlBlock(T* p, void (*d)(T*) = nullptr)
        : ptr(p), ref_count(1), deleter(d), default_destroy_(&DefaultDelete<T>) {}
    void release() {
        if (ref_count.fetch_sub(1) == 1) {
            // Llamar a traves de un puntero a funcion de firma void(T*)
            // NO exige T completo (solo pasa el valor del puntero) -- por
            // eso este branch nunca tuvo el problema. Solo `delete ptr`
            // (default_destroy_, type-erased) lo tenia.
            if (deleter) deleter(ptr); else default_destroy_(ptr);
            delete this;
        }
    }
};

template <class T>
class shared_ptr {
public:
    shared_ptr() noexcept : block_(nullptr) {}
    shared_ptr(avastd::nullptr_t) noexcept : block_(nullptr) {}
    explicit shared_ptr(T* p) : block_(p ? new ControlBlock<T>(p) : nullptr) {}

    // shared_ptr con deleter custom (patron "aliasing sin ownership real",
    // ver vm_call_op.cpp: shared_ptr a un Closure que vive en otro lado,
    // solo para calzar una firma que pide shared_ptr<Closure>). Solo
    // soporta deleters SIN estado (function pointer puro, no lambdas con
    // captura) -- es la unica forma que usa AvaLang hoy.
    template <class D>
    shared_ptr(T* p, D deleter) : block_(p ? new ControlBlock<T>(p, static_cast<void(*)(T*)>(deleter)) : nullptr) {}

    shared_ptr(const shared_ptr& other) noexcept : block_(other.block_) {
        if (block_) block_->ref_count.fetch_add(1);
    }
    shared_ptr(shared_ptr&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

    // Permite shared_ptr<Base> desde shared_ptr<Derived> (upcast implicito,
    // igual que std::shared_ptr).
    template <class U>
    shared_ptr(const shared_ptr<U>& other) noexcept : block_(reinterpret_cast<ControlBlock<T>*>(other.block_raw())) {
        if (block_) block_->ref_count.fetch_add(1);
    }

    ~shared_ptr() { if (block_) block_->release(); }

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this == &other) return *this;
        if (other.block_) other.block_->ref_count.fetch_add(1);
        if (block_) block_->release();
        block_ = other.block_;
        return *this;
    }
    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this == &other) return *this;
        if (block_) block_->release();
        block_ = other.block_;
        other.block_ = nullptr;
        return *this;
    }
    shared_ptr& operator=(avastd::nullptr_t) noexcept {
        if (block_) block_->release();
        block_ = nullptr;
        return *this;
    }

    T* get() const noexcept { return block_ ? block_->ptr : nullptr; }
    T& operator*() const noexcept { return *block_->ptr; }
    T* operator->() const noexcept { return block_->ptr; }
    explicit operator bool() const noexcept { return block_ != nullptr; }

    long use_count() const noexcept { return block_ ? block_->ref_count.load() : 0; }
    void reset() { if (block_) { block_->release(); block_ = nullptr; } }

    friend bool operator==(const shared_ptr& a, const shared_ptr& b) { return a.block_ == b.block_; }
    friend bool operator!=(const shared_ptr& a, const shared_ptr& b) { return a.block_ != b.block_; }
    friend bool operator==(const shared_ptr& a, avastd::nullptr_t) { return a.block_ == nullptr; }
    friend bool operator!=(const shared_ptr& a, avastd::nullptr_t) { return a.block_ != nullptr; }

    ControlBlock<T>* block_raw() const noexcept { return block_; }

private:
    template <class U> friend class shared_ptr;
    ControlBlock<T>* block_;
};

template <class T, class... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(new T(avastd::forward<Args>(args)...));
}

// unique_ptr: ownership exclusivo simple (sin soporte de deleter
// customizado ni arrays -- AvaLang no los necesita hoy, ver grep de uso).
template <class T>
class unique_ptr {
public:
    unique_ptr() noexcept : ptr_(nullptr) {}
    unique_ptr(avastd::nullptr_t) noexcept : ptr_(nullptr) {}
    explicit unique_ptr(T* p) noexcept : ptr_(p) {}
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    // Permite unique_ptr<Base> desde unique_ptr<Derived> (upcast implicito
    // por move, igual que std::unique_ptr) -- hueco encontrado en
    // BareKernelPlatform::Create(), que hace
    // `return avastd::make_unique<BareKernelPlatform>();` contra un
    // `unique_ptr<IPlatform>` de retorno (ver §11 del audit).
    template <class U>
    unique_ptr(unique_ptr<U>&& other) noexcept : ptr_(other.release()) {}
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this == &other) return *this;
        delete ptr_;
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        return *this;
    }

    ~unique_ptr() { delete ptr_; }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* release() noexcept { T* p = ptr_; ptr_ = nullptr; return p; }
    void reset(T* p = nullptr) noexcept { delete ptr_; ptr_ = p; }

private:
    T* ptr_;
};

template <class T, class... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(avastd::forward<Args>(args)...));
}

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_SHARED_PTR_H
