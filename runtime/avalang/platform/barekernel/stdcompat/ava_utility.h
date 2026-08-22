#ifndef AVA_STDCOMPAT_UTILITY_H
#define AVA_STDCOMPAT_UTILITY_H

#include "ava_platform_caps.h"

// move/forward/pair/swap no dependen de libstdc++ en absoluto -- son
// plantillas puras que el estandar podria haber puesto en cualquier header
// freestanding. Cuando hay STL real los usamos directo desde <utility>
// (cero costo, exactamente el mismo codigo). Cuando no, esta es una
// reimplementacion byte-a-byte del comportamiento estandar.

#if AVA_HAVE_STD_LIBRARY

#include <utility>
namespace avastd {
using std::move;
using std::forward;
using std::pair;
using std::make_pair;
using std::swap;
using std::remove_reference;
using std::remove_reference_t;
using std::enable_if;
using std::enable_if_t;
using std::is_same;
using std::declval;
}  // namespace avastd

#else

namespace avastd {

template <class T> struct remove_reference       { using type = T; };
template <class T> struct remove_reference<T&>   { using type = T; };
template <class T> struct remove_reference<T&&>  { using type = T; };
template <class T> using remove_reference_t = typename remove_reference<T>::type;

template <bool B, class T = void> struct enable_if {};
template <class T> struct enable_if<true, T> { using type = T; };
template <bool B, class T = void> using enable_if_t = typename enable_if<B, T>::type;

template <class T, class U> struct is_same       { static constexpr bool value = false; };
template <class T>          struct is_same<T, T> { static constexpr bool value = true; };

template <class T>
T&& declval() noexcept;  // solo para contexto no evaluado (decltype/sizeof)

template <class T>
constexpr remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_reference_t<T>&&>(t);
}

template <class T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}
template <class T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
    return static_cast<T&&>(t);
}

template <class T>
void swap(T& a, T& b) noexcept {
    T tmp = move(a);
    a = move(b);
    b = move(tmp);
}

template <class A, class B>
struct pair {
    A first;
    B second;
    pair() = default;
    pair(const A& a, const B& b) : first(a), second(b) {}
    pair(A&& a, B&& b) : first(move(a)), second(move(b)) {}
    template <class A2, class B2>
    pair(const pair<A2, B2>& other) : first(other.first), second(other.second) {}
};

template <class A, class B>
pair<A, B> make_pair(A a, B b) { return pair<A, B>(move(a), move(b)); }

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_UTILITY_H
