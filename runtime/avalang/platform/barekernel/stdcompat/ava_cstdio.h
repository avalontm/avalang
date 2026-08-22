#ifndef AVA_STDCOMPAT_CSTDIO_H
#define AVA_STDCOMPAT_CSTDIO_H

#include "ava_platform_caps.h"
#include "ava_types.h"

// snprintf no tenia ningun modulo propio en avastd -- vm_extern.cpp lo usa
// (formatear el codigo de excepcion de un crash nativo, "0x%08lX") pero
// nunca se agrego el alias/implementacion, asi que en build hosted
// (CKM_CAP_LIBSTDCPP=1) fallaba con C2039 "snprintf no es un miembro de
// avastd". Mismo patron que el resto de stdcompat/: alias directo a la STL
// real cuando hay, implementacion propia minima cuando no.

#if AVA_HAVE_STD_LIBRARY

#include <cstdio>
namespace avastd {
using std::snprintf;
}  // namespace avastd

#else

namespace avastd {

// Subconjunto de snprintf suficiente para lo que AvaLang necesita
// (formatear codigos de error/excepcion, contadores, etc.): %d/%i, %u,
// %x/%X, %o, %c, %s, %p, %%, con soporte de ancho + zero-padding
// (`%08lX`) y los modificadores de longitud l/ll/z. No soporta precision
// (`%.2f`), flags de signo (+/space), ni conversion de punto flotante
// (%f/%e/%g) -- nada en el arbol los usa via avastd::snprintf hoy; si
// hiciera falta, se agrega cuando aparezca el primer call site real, no
// antes.
inline int snprintf(char* buf, avastd::size_t bufsize, const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    avastd::size_t out = 0;
    auto put = [&](char c) {
        if (out + 1 < bufsize) buf[out] = c;
        ++out;
    };
    auto put_str = [&](const char* s) {
        while (*s) put(*s++);
    };

    const char* digits_lower = "0123456789abcdef";
    const char* digits_upper = "0123456789ABCDEF";

    for (const char* p = fmt; *p; ++p) {
        if (*p != '%') { put(*p); continue; }
        ++p;
        if (*p == '\0') break;

        // Flags/ancho minimo: solo lo que vm_extern.cpp usa ("%08lX").
        bool zero_pad = false;
        while (*p == '0') { zero_pad = true; ++p; }
        int width = 0;
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); ++p; }

        // Modificadores de longitud: se leen pero todo termina promovido
        // a long/unsigned long para el formateo (suficiente en esta arbol).
        bool is_long = false;
        while (*p == 'l' || *p == 'z' || *p == 'h') { if (*p == 'l') is_long = true; ++p; }

        char conv = *p;
        char num_buf[32];
        int num_len = 0;

        auto emit_unsigned = [&](unsigned long v, int base, const char* digit_set) {
            num_len = 0;
            if (v == 0) { num_buf[num_len++] = '0'; }
            while (v != 0) {
                num_buf[num_len++] = digit_set[v % static_cast<unsigned long>(base)];
                v /= static_cast<unsigned long>(base);
            }
            int pad = width - num_len;
            for (int i = 0; i < pad; ++i) put(zero_pad ? '0' : ' ');
            for (int i = num_len - 1; i >= 0; --i) put(num_buf[i]);
        };

        switch (conv) {
            case 'd':
            case 'i': {
                long v = is_long ? __builtin_va_arg(args, long)
                                  : static_cast<long>(__builtin_va_arg(args, int));
                unsigned long u = v < 0 ? static_cast<unsigned long>(-(v + 1)) + 1
                                         : static_cast<unsigned long>(v);
                if (v < 0) put('-');
                emit_unsigned(u, 10, digits_lower);
                break;
            }
            case 'u':
                emit_unsigned(is_long ? __builtin_va_arg(args, unsigned long)
                                       : static_cast<unsigned long>(__builtin_va_arg(args, unsigned int)),
                              10, digits_lower);
                break;
            case 'x':
                emit_unsigned(is_long ? __builtin_va_arg(args, unsigned long)
                                       : static_cast<unsigned long>(__builtin_va_arg(args, unsigned int)),
                              16, digits_lower);
                break;
            case 'X':
                emit_unsigned(is_long ? __builtin_va_arg(args, unsigned long)
                                       : static_cast<unsigned long>(__builtin_va_arg(args, unsigned int)),
                              16, digits_upper);
                break;
            case 'o':
                emit_unsigned(is_long ? __builtin_va_arg(args, unsigned long)
                                       : static_cast<unsigned long>(__builtin_va_arg(args, unsigned int)),
                              8, digits_lower);
                break;
            case 'p': {
                put('0'); put('x');
                emit_unsigned(reinterpret_cast<unsigned long>(__builtin_va_arg(args, void*)), 16, digits_lower);
                break;
            }
            case 'c':
                put(static_cast<char>(__builtin_va_arg(args, int)));
                break;
            case 's':
                put_str(__builtin_va_arg(args, const char*));
                break;
            case '%':
                put('%');
                break;
            default:
                put('%');
                put(conv);
                break;
        }
    }

    __builtin_va_end(args);

    if (bufsize > 0) buf[out < bufsize ? out : bufsize - 1] = '\0';
    return static_cast<int>(out);
}

}  // namespace avastd

#endif  // AVA_HAVE_STD_LIBRARY

#endif  // AVA_STDCOMPAT_CSTDIO_H
