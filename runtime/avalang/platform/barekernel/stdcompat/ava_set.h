#ifndef AVA_STDCOMPAT_SET_H
#define AVA_STDCOMPAT_SET_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_vector.h"

#if AVA_HAVE_STD_LIBRARY

#include <set>
namespace avastd {
template <class T>
using set = std::set<T>;
}

#else

namespace avastd {

// Set ordenado, unico consumidor hoy es obfuscate.cpp (ComputeBlocks:
// insertar leaders y recorrerlos ya ordenados). Vector ordenado + insert
// binario en vez de un arbol balanceado: O(n) por insert, aceptable para
// los tamanos reales (instrucciones de una funcion, no millones de items).
template <class T>
class set {
public:
    using const_iterator = const T*;

    void insert(const T& v) {
        avastd::size_t lo = 0, hi = data_.size();
        while (lo < hi) {
            avastd::size_t mid = (lo + hi) / 2;
            if (data_[mid] < v) lo = mid + 1; else hi = mid;
        }
        if (lo < data_.size() && data_[lo] == v) return;
        data_.push_back(v);
        for (avastd::size_t i = data_.size() - 1; i > lo; --i) data_[i] = data_[i - 1];
        data_[lo] = v;
    }

    const_iterator begin() const { return data_.data(); }
    const_iterator end() const { return data_.data() + data_.size(); }
    avastd::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }

private:
    avastd::vector<T> data_;
};

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_SET_H
