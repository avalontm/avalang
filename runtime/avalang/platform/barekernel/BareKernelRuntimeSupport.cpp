// BareKernelRuntimeSupport.cpp
//
// Runtime support que este toolchain freestanding (i686-elf,
// --without-headers, -nostdlib, sin libstdc++/libsupc++/libm) no trae, y
// que libavalang.so necesita igual porque LibraryLoader::ApplyRelocation
// (lib_loader.cpp del kernel) resuelve simbolos SOLO contra el symtab del
// propio .so, nunca contra otras bibliotecas ya cargadas (ver
// docs/kernel/binding-status.md) -- nada de esto se puede "pedir prestado"
// al runner en tiempo de carga, tiene que vivir aca adentro.
//
// Dos problemas independientes, ambos descubiertos recien en el link real
// (build_barekernel.bat), no con el -fsyntax-only usado hasta ahora para
// verificar sintaxis sin el cross-compiler:
//
// 1) mem*/str*: el codigo de AvaLang para este target ya tiene sus propias
//    avastd::memcpy/avastd::strlen/avastd::memcmp (ver ava_string.h,
//    implementadas a mano, sin <cstring>) y las usa explicitamente. Eso
//    NO alcanza: GCC, de forma completamente independiente de lo que el
//    codigo fuente pide, genera llamadas IMPLICITAS a los simbolos C
//    "pelados" `memcpy`/`memmove`/`memset`/`strlen` (no `avastd::algo`)
//    para cosas como inicializacion de arrays/structs en cero, copia de
//    objetos grandes, y sobre todo "loop-idiom recognition"
//    (-O2/-O3 -ftree-loop-distribute-patterns): un loop escrito a mano
//    que copia bytes uno a uno -- exactamente lo que
//    avastd::memcpy/strlen tienen adentro -- puede terminar reescrito por
//    el propio optimizador como una llamada a `memcpy`/`strlen`. Sin una
//    libc real proveyendolos, hacen falta ACA, con nombres C sin
//    decorar, sin relacion con el namespace avastd:: (esto no duplica esa
//    logica "porque si": satisface un segundo llamador implicito, el
//    propio compilador, que avastd:: no puede interceptar).
//
// 2) round/sqrt/fmod/pow: ver el comentario en ava_math.h -- a diferencia
//    de fabs/floor/ceil/trunc (que SI bajan a una sola instruccion de
//    FPU/SSE via __builtin_*, confirmado: no aparecen en la lista de
//    simbolos sin resolver del link real), estas cuatro terminan
//    generando una llamada al simbolo libm de mismo nombre cuando el
//    valor no se puede resolver en tiempo de compilacion (sqrt ademas
//    entra por otra puerta: por default GCC respeta errno en domain
//    errors de sqrt() y NO la inlinea a la instruccion de FPU a menos que
//    se compile con -fno-math-errno/-ffast-math, ninguna de las cuales
//    esta puesta aca). Implementadas a mano via x87 (target real:
//    i686-elf -m32, sin -msse2 -- ver cmake/toolchain-i686-elf.cmake --
//    asi que x87 es lo unico garantizado disponible).
//
// IMPORTANTE -- por que esto necesita -fno-builtin en TODO el target
// avalang (ver runtime/avalang/CMakeLists.txt, rama AVA_TARGET_BAREKERNEL,
// no solo en este archivo): sin esa flag, GCC es libre de reconocer los
// cuerpos de memcpy/memset/strlen de mas abajo como instancias del propio
// patron que estan implementando, y reescribirlos como llamadas a si
// mismos -- recursion infinita en runtime, unicamente en Release, y
// unicamente en este target (en Windows/Linux/macOS estas funciones ni se
// compilan, AVA_HAVE_STD_LIBRARY=1 usa <cstring>/<cmath> reales). Y como
// el problema de fondo (el compilador generando una llamada implicita a
// un simbolo que este toolchain sin libc no tiene) puede originarse en
// CUALQUIER .cpp del target, no solo en este archivo, la flag se aplica a
// todo avalang, no target_compile_options por-archivo aca.

#include "stdcompat/ava_platform_caps.h"

#if !AVA_HAVE_STD_LIBRARY

#include "stdcompat/ava_types.h"

// ---------------------------------------------------------------------
// mem*/str* -- ver punto 1) arriba. Implementaciones byte a byte,
// deliberadamente simples (no vectorizadas): correctitud primero en un
// target que todavia no se probo contra el kernel real (ver "Pendiente
// real" en docs/kernel/binding-status.md); optimizarlas es trabajo
// futuro si el profiling en QEMU/hardware real algun dia lo pide.
// ---------------------------------------------------------------------

extern "C" {

void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dst);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dst);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    if (d == s || n == 0) return dst;
    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dst;
}

void* memset(void* dst, int value, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dst);
    unsigned char v = static_cast<unsigned char>(value);
    for (size_t i = 0; i < n; ++i) d[i] = v;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = static_cast<const unsigned char*>(a);
    const unsigned char* pb = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) return static_cast<int>(pa[i]) - static_cast<int>(pb[i]);
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

}  // extern "C"

// ---------------------------------------------------------------------
// libm gap -- ver punto 2) arriba. Idioms x87 estandar (los mismos que
// usan glibc/musl en sysdeps/i386 para esto mismo, sin libc real de
// donde tomarlos prestados aca).
// ---------------------------------------------------------------------

extern "C" {

double sqrt(double x) {
    double result;
    __asm__ __volatile__("fsqrt" : "=t"(result) : "0"(x));
    return result;
}

double round(double x) {
    // Semantica de C round(): mitad-lejos-de-cero. floor/ceil SI foldean
    // a una sola instruccion de FPU via __builtin_* explicito (no pasan
    // por el nombre "floor"/"ceil" pelado, que dispararia exactamente el
    // mismo problema que este archivo resuelve para round/sqrt/pow/fmod
    // -- por eso son seguros de llamar desde aca).
    return (x >= 0.0) ? __builtin_floor(x + 0.5) : __builtin_ceil(x - 0.5);
}

double fmod(double x, double y) {
    // FPREM calcula un resto parcial si el exponente de x supera al de y
    // por mas de 64 -- hay que repetir hasta que el flag C2 de la status
    // word de la FPU (bit reflejado en el flag de paridad tras fnstsw+
    // sahf) se limpie. Bucle estandar para esto en i386.
    double result;
    unsigned short fpu_status;
    __asm__ __volatile__(
        "1: fprem\n\t"
        "fnstsw %%ax\n\t"
        "sahf\n\t"
        "jp 1b\n\t"
        : "=t"(result), "=a"(fpu_status)
        : "0"(x), "u"(y)
        : "cc");
    return result;
}

double pow(double base, double exponent) {
    if (exponent == 0.0) return 1.0;
    if (base == 0.0) return (exponent > 0.0) ? 0.0 : __builtin_huge_val();

    // Camino rapido y EXACTO para exponente entero: cubre la enorme
    // mayoria de usos reales de `**`/pow() en scripts AvaLang (x**2,
    // x**3, ...) y es ademas el UNICO camino de los dos que funciona con
    // base negativa (el camino general de abajo, via log2/exp2, no esta
    // definido ahi -- ni en libm real).
    if (exponent == __builtin_trunc(exponent) &&
        exponent > -1024.0 && exponent < 1024.0) {
        long long n = static_cast<long long>(exponent);
        bool neg = n < 0;
        if (neg) n = -n;
        double result = 1.0;
        double b = base;
        while (n) {
            if (n & 1) result *= b;
            b *= b;
            n >>= 1;
        }
        return neg ? 1.0 / result : result;
    }

    if (base < 0.0) {
        // Potencia fraccionaria de base negativa: resultado no real, no
        // representable en double. NaN es lo mismo que devuelve libm
        // real aca (via 0.0/0.0, sin necesitar una constante NaN aparte).
        double zero = 0.0;
        return zero / zero;
    }

    // base^exponent = 2^(exponent * log2(base)) -- via las dos
    // instrucciones x87 que existen justo para esto (FYL2X, F2XM1).
    // Mismo idiom que usan glibc/musl en sysdeps/i386 para pow/exp2.
    double log2_base;
    __asm__ __volatile__(
        "fld1\n\t"
        "fxch\n\t"
        "fyl2x\n\t"
        : "=t"(log2_base)
        : "0"(base));

    double y = exponent * log2_base;
    double result;
    __asm__ __volatile__(
        "fld %%st(0)\n\t"
        "frndint\n\t"
        "fsub %%st(0), %%st(1)\n\t"
        "fxch\n\t"
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"
        "fscale\n\t"
        "fstp %%st(1)\n\t"
        : "=t"(result)
        : "0"(y)
        : "st(1)");
    return result;
}

}  // extern "C"

// ---------------------------------------------------------------------
// ABI minima de C++ freestanding, sin libstdc++/libsupc++.
//
// __gxx_personality_v0 y las vtables de
// __cxxabiv1::__class_type_info/__si_class_type_info (el otro bloque de
// simbolos sin resolver del link real) NO se resuelven aca -- se evitan
// en origen con -fno-exceptions -fno-rtti al COMPILAR (ver CMakeLists.txt,
// target avalang, rama AVA_TARGET_BAREKERNEL). La documentacion de GCC
// para -fno-rtti es explicita: "exception handling uses the same
// information, but G++ generates it as needed" -- sin excepciones
// (-fno-exceptions) tampoco hace falta esa informacion. Con las dos
// puestas, ese bloque completo de simbolos deja de generarse.
//
// __cxa_guard_acquire/_release/_abort (el trio para inicializacion
// thread-safe de estaticas locales no-POD, como el Meyer's singleton de
// VmPlatformAccessor::Get()) se evitan igual, con -fno-threadsafe-statics
// en el mismo lugar -- correcto porque CKM_CAP_THREADS=0 (ver
// binding.cmake), no hay ejecucion paralela real que proteger.
//
// Lo que SI sigue haciendo falta pase lo que pase con esas flags:
// registrar destructores de objetos estaticos con inicializacion no
// trivial (locales Y globales, estos ultimos via _GLOBAL__sub_I_*) llama
// a __cxa_atexit, identificando el modulo con __dso_handle. Se definen
// aca porque no hay libc/runtime del host que los provea:
//
// __cxa_atexit no-op (retorna exito sin registrar nada de verdad): el
// kernel no tiene hoy un punto de "shutdown" que corra destructores
// globales de un .so cargado dinamicamente (ILibraryLoader/
// BareKernelLibrary.cpp no invocan nada asi al descargar), asi que
// ninguno de los destructores de esta lista se ejecutaria de verdad de
// todas formas -- omitir el registro es equivalente, en comportamiento
// observable, a registrarlo y no llamarlo nunca.
extern "C" void* __dso_handle = nullptr;

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

#endif  // !AVA_HAVE_STD_LIBRARY
