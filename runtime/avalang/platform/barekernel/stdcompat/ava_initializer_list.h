#ifndef AVA_STDCOMPAT_INITIALIZER_LIST_H
#define AVA_STDCOMPAT_INITIALIZER_LIST_H

#include "ava_platform_caps.h"
#include "ava_types.h"

// `std::initializer_list<T>` es un tipo especial reconocido por nombre
// por el front-end de GCC/Clang: cuando el codigo escribe `{a, b, c}` en
// un contexto que espera un tipo con constructor
// `initializer_list<T>` (o directamente ese tipo), el compilador arma un
// array temporal `const T[]` en algun lugar con storage duration
// apropiado y construye el `std::initializer_list<T>` llamando a un
// constructor de dos argumentos `(const T*, size_t)` que SOLO EL
// COMPILADOR invoca -- no hace falta implementar nada mas.
//
// Con libstdc++ real (CKM_CAP_LIBSTDCPP=1) esto viene de <initializer_list>,
// que a su vez requiere una libc/libstdc++ instalada. Este toolchain es
// `--without-headers` (ver el razonamiento identico en ava_types.h): no
// hay <initializer_list> para incluir. La solucion es la misma que ya usa
// el resto de stdcompat/: escribir el tipo a mano.
//
// El layout de abajo (puntero primero, size_t segundo, nombres
// _M_array/_M_len, constructor de dos argumentos privado) replica
// exactamente el de libstdc++ porque versiones de GCC/Clang que no leen
// el header real (como este, que no tiene ninguno) asumen ese layout al
// generar el codigo que llama al constructor privado -- no es un detalle
// cosmetico, es el contrato binario que el compilador ya trae grabado.

#if AVA_HAVE_STD_LIBRARY

#include <initializer_list>

#else

namespace std {

template <class E>
class initializer_list {
public:
    using value_type = E;
    using reference = const E&;
    using const_reference = const E&;
    using size_type = avastd::size_t;
    using iterator = const E*;
    using const_iterator = const E*;

    constexpr initializer_list() noexcept : _M_array(nullptr), _M_len(0) {}

    constexpr size_type size() const noexcept { return _M_len; }
    constexpr const_iterator begin() const noexcept { return _M_array; }
    constexpr const_iterator end() const noexcept { return _M_array + _M_len; }

private:
    // Llamado unicamente por el compilador al desazucarar una
    // lista `{...}` -- ver el comentario de arriba. Se mantiene
    // privado, igual que libstdc++, porque el compilador lo invoca
    // directamente sin pasar por el control de acceso normal.
    constexpr initializer_list(const_iterator array, size_type len) noexcept
        : _M_array(array), _M_len(len) {}

    iterator _M_array;
    size_type _M_len;
};

template <class T>
constexpr const T* begin(initializer_list<T> il) noexcept { return il.begin(); }

template <class T>
constexpr const T* end(initializer_list<T> il) noexcept { return il.end(); }

}  // namespace std

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_INITIALIZER_LIST_H
