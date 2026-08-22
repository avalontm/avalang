#ifndef AVA_STDCOMPAT_VECTOR_H
#define AVA_STDCOMPAT_VECTOR_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_utility.h"
#include "ava_new.h"
#include "../../AvaMemory.h"

#if AVA_HAVE_STD_LIBRARY

#include <vector>
namespace avastd {
template <class T> using vector = std::vector<T>;
}

#else

namespace avastd {

// Vector con crecimiento geometrico (x2) sobre ava_alloc/ava_realloc/ava_free
// (platform/AvaMemory.h). Sin excepciones: si ava_alloc falla, el
// comportamiento es el mismo que ya tiene esa implementacion ante OOM (no
// se agrega un segundo mecanismo de fallo aca). Cubre la superficie que usa
// AvaLang hoy: push_back/emplace_back/pop_back, operator[], size/empty/
// clear/reserve/resize, begin/end (range-for), y el patron "vector de
// vector"/"vector de shared_ptr" (requiere que T no-trivial se mueva/
// destruya bien).

template <class T>
class vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    vector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    vector(const vector& other) : data_(nullptr), size_(0), capacity_(0) {
        reserve(other.size_);
        for (avastd::size_t i = 0; i < other.size_; ++i) push_back(other.data_[i]);
    }

    vector(vector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    explicit vector(avastd::size_t count) : data_(nullptr), size_(0), capacity_(0) {
        resize(count);
    }

    // Constructor por rango [first, last) -- cubre el patron
    // `vector<T> sub(v.begin(), v.begin() + n)` que usa AvaLang para
    // "slicing" de vectores (ver vm_import.cpp).
    template <class It>
    vector(It first, It last) : data_(nullptr), size_(0), capacity_(0) {
        for (; first != last; ++first) push_back(*first);
    }

    ~vector() { clear_and_free(); }

    vector& operator=(const vector& other) {
        if (this == &other) return *this;
        clear();
        reserve(other.size_);
        for (avastd::size_t i = 0; i < other.size_; ++i) push_back(other.data_[i]);
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this == &other) return *this;
        clear_and_free();
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }

    void reserve(avastd::size_t new_cap) {
        if (new_cap <= capacity_) return;
        T* new_data = static_cast<T*>(ava_alloc(new_cap * sizeof(T)));
        for (avastd::size_t i = 0; i < size_; ++i) {
            new (&new_data[i]) T(avastd::move(data_[i]));
            data_[i].~T();
        }
        if (data_) ava_free(data_);
        data_ = new_data;
        capacity_ = new_cap;
    }

    void push_back(const T& value) {
        ensure_capacity_for_one();
        new (&data_[size_]) T(value);
        ++size_;
    }
    void push_back(T&& value) {
        ensure_capacity_for_one();
        new (&data_[size_]) T(avastd::move(value));
        ++size_;
    }
    template <class... Args>
    T& emplace_back(Args&&... args) {
        ensure_capacity_for_one();
        new (&data_[size_]) T(avastd::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back() {
        if (size_ == 0) return;
        --size_;
        data_[size_].~T();
    }

    void resize(avastd::size_t new_size) {
        if (new_size < size_) {
            for (avastd::size_t i = new_size; i < size_; ++i) data_[i].~T();
            size_ = new_size;
            return;
        }
        reserve(new_size);
        for (avastd::size_t i = size_; i < new_size; ++i) new (&data_[i]) T();
        size_ = new_size;
    }

    void clear() {
        for (avastd::size_t i = 0; i < size_; ++i) data_[i].~T();
        size_ = 0;
    }

    T&       operator[](avastd::size_t i)       { return data_[i]; }
    const T& operator[](avastd::size_t i) const { return data_[i]; }
    T&       at(avastd::size_t i)       { return data_[i]; }
    const T& at(avastd::size_t i) const { return data_[i]; }
    T&       front()       { return data_[0]; }
    const T& front() const { return data_[0]; }
    T&       back()        { return data_[size_ - 1]; }
    const T& back() const  { return data_[size_ - 1]; }

    avastd::size_t size() const noexcept { return size_; }
    avastd::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    iterator begin() noexcept { return data_; }
    iterator end() noexcept { return data_ + size_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator end() const noexcept { return data_ + size_; }

    iterator erase(iterator pos) {
        avastd::size_t idx = static_cast<avastd::size_t>(pos - data_);
        data_[idx].~T();
        for (avastd::size_t i = idx; i + 1 < size_; ++i) {
            new (&data_[i]) T(avastd::move(data_[i + 1]));
            data_[i + 1].~T();
        }
        --size_;
        return data_ + idx;
    }

    // insert(pos, value): un solo elemento antes de pos. Encontrado como
    // hueco real via el syntax-check freestanding al compilar api/src/c_api.cpp
    // (ava_list_insert llama items.insert(pos, valor_unico) -- std::vector
    // hosted lo resuelve por su propio overload de valor unico, que esta
    // implementacion freestanding no tenia, solo el de rango). Se apoya en
    // el insert(pos, first, last) de arriba con un rango de un puntero,
    // sin duplicar la logica de desplazamiento.
    iterator insert(iterator pos, const T& value) {
        return insert(pos, &value, &value + 1);
    }

    // insert(pos, first, last): inserta el rango [first,last) antes de
    // pos. AvaLang lo usa solo con pos == end() (append de otro
    // contenedor, ver vm_call_op.cpp) -- se optimiza ese caso comun sin
    // perder correccion para pos intermedio.
    template <class It>
    iterator insert(iterator pos, It first, It last) {
        avastd::size_t idx = static_cast<avastd::size_t>(pos - data_);
        for (; first != last; ++first, ++idx) {
            if (idx >= size_) {
                push_back(*first);
            } else {
                // Desplazar todo un lugar a la derecha, luego colocar.
                emplace_back(avastd::move(back()));
                for (avastd::size_t i = size_ - 1; i > idx; --i) {
                    data_[i] = avastd::move(data_[i - 1]);
                }
                data_[idx] = *first;
            }
        }
        return data_ + idx;
    }

    void swap(vector& other) noexcept {
        T* d = data_; data_ = other.data_; other.data_ = d;
        avastd::size_t s = size_; size_ = other.size_; other.size_ = s;
        avastd::size_t c = capacity_; capacity_ = other.capacity_; other.capacity_ = c;
    }

private:
    void ensure_capacity_for_one() {
        if (size_ < capacity_) return;
        reserve(capacity_ == 0 ? 4 : capacity_ * 2);
    }
    void clear_and_free() {
        clear();
        if (data_) ava_free(data_);
        data_ = nullptr;
        capacity_ = 0;
    }

    T* data_;
    avastd::size_t size_;
    avastd::size_t capacity_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_VECTOR_H
