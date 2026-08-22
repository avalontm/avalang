#ifndef AVA_STDCOMPAT_ALGORITHM_H
#define AVA_STDCOMPAT_ALGORITHM_H

#include "ava_platform_caps.h"
#include "ava_utility.h"

// A diferencia de <atomic>/<vector>/etc., <algorithm> en libstdc++ real
// tampoco necesita "runtime" -- son plantillas puras igual que las de
// ava_utility.h. Se separa en su propio header de todos modos (no se
// mete en ava_utility.h) porque sort() en particular es codigo no
// trivial y no todos los archivos que usan move/forward/pair necesitan
// tambien sort/find.

#if AVA_HAVE_STD_LIBRARY

#include <algorithm>
namespace avastd {
using std::sort;
using std::find;
using std::find_if;
using std::min;
using std::max;
using std::clamp;
using std::reverse;
using std::count_if;
using std::any_of;
using std::all_of;
using std::transform;
}

#else

namespace avastd {

template <class T>
const T& min(const T& a, const T& b) { return (b < a) ? b : a; }
template <class T>
const T& max(const T& a, const T& b) { return (a < b) ? b : a; }
template <class T>
const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

template <class It, class T>
It find(It first, It last, const T& value) {
    for (; first != last; ++first) if (*first == value) return first;
    return last;
}
template <class It, class Pred>
It find_if(It first, It last, Pred pred) {
    for (; first != last; ++first) if (pred(*first)) return first;
    return last;
}
template <class It, class Pred>
bool any_of(It first, It last, Pred pred) {
    for (; first != last; ++first) if (pred(*first)) return true;
    return false;
}
template <class It, class Pred>
bool all_of(It first, It last, Pred pred) {
    for (; first != last; ++first) if (!pred(*first)) return false;
    return true;
}
template <class It, class Pred>
avastd::ptrdiff_t count_if(It first, It last, Pred pred) {
    avastd::ptrdiff_t n = 0;
    for (; first != last; ++first) if (pred(*first)) ++n;
    return n;
}

template <class It>
void reverse(It first, It last) {
    while (first != last && first != --last) {
        avastd::swap(*first, *last);
        ++first;
    }
}

// Insertion sort: O(n^2), pero simple y correcto, y AvaLang ordena
// colecciones de scripts (decenas/cientos de elementos, no millones) --
// no el cuello de botella esperado. Si un profiling real en el kernel
// muestra lo contrario, reemplazar por quicksort/introsort sin tocar la
// firma (mismo patron que se dejo documentado en ava_vector.h/ava_string.h
// para sus propias optimizaciones futuras).
template <class It, class Cmp>
void sort(It first, It last, Cmp cmp) {
    for (It i = first; i != last; ++i) {
        for (It j = i; j != first; ) {
            It prev = j;
            --prev;
            if (cmp(*j, *prev)) {
                avastd::swap(*j, *prev);
                j = prev;
            } else {
                break;
            }
        }
    }
}
template <class It>
void sort(It first, It last) {
    sort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_ALGORITHM_H
