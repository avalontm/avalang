#ifndef AVA_STDCOMPAT_ERROR_H
#define AVA_STDCOMPAT_ERROR_H

#include "ava_platform_caps.h"
#include "ava_types.h"
#include "ava_string.h"
#include "ava_utility.h"

// Ver docs/kernel/PLAN_BAREKERNEL_STDCOMPAT.md, seccion "Manejo de
// errores", para el razonamiento completo. Resumen:
//
//   CKM_CAP_STD_EXCEPTIONS=1 -> AVA_TRY/AVA_THROW/AVA_CATCH son alias
//   directos de try/throw/catch reales. Cero cambio de comportamiento en
//   Windows/Linux/macOS.
//
//   CKM_CAP_STD_EXCEPTIONS=0 -> se implementan con setjmp/longjmp.
//   LIMITACION ACEPTADA Y DOCUMENTADA: longjmp NO llama destructores de
//   objetos automaticos entre el AVA_TRY y el AVA_THROW. Aceptable porque
//   el unico lugar donde esto se dispara es la ruta de ERROR FATAL del
//   VM (abortar la ejecucion de un script), no la ruta de exito.
//
//   Sin RTTI real (dynamic_cast no funciona en este kernel -- ver
//   docs/kernel/kernel.md §2.5), AVA_CATCH no discrimina por tipo como un
//   catch() real: siempre "atrapa" lo ultimo lanzado dentro del AVA_TRY,
//   casteado (no dynamic_cast, static_cast) al tipo declarado. Por eso la
//   forma es AVA_CATCH(Tipo, nombre) -- dos argumentos, no
//   AVA_CATCH(Tipo& nombre) como un catch real -- para no fingir una
//   garantia de seguridad de tipos que este runtime no puede dar. Si
//   AvaLang lanza mas de un tipo de error dentro del mismo AVA_TRY y
//   necesita discriminar, agregar un `int type_tag` a exception y
//   chequearlo a mano (mas simple y mas barato que RTTI real aca).

#if AVA_HAVE_EXCEPTIONS

#include <exception>
#include <stdexcept>

namespace avastd {
using exception = std::exception;
using runtime_error = std::runtime_error;
}

#define AVA_TRY               try
#define AVA_CATCH(Type, name) catch (Type& name)
#define AVA_THROW(expr)       throw (expr)
#define AVA_RETHROW()         throw

#else

// setjmp.h TAMBIEN resulto ser un header provisto por libc (no por el
// compilador) -- con -nostdinc, falla exactamente igual que fallaba
// stdint.h antes de corregir ava_types.h. Este toolchain no tiene libc
// (--without-headers), asi que no hay setjmp/longjmp para tomar prestados.
//
// Implementacion propia para i386 (arquitectura real del target, ver
// build_barekernel.bat: i686-elf-*), en ensamblador inline, validada
// contra codigo real (no solo "deberia andar"): un test standalone de 32
// bits, corrido en este entorno con gcc -m32 en -O0/-O2/-O3
// -fomit-frame-pointer, hace setjmp, recursiona 6 niveles quemando stack
// a proposito, y longjmp de vuelta -- confirma que el valor de retorno y
// el estado de variables previas al setjmp sobreviven correctos. La
// tecnica (guardar ebx/esi/edi/ebp/esp/eip, calling convention cdecl) es
// la misma que usan libcs freestanding reales (musl, dietlibc, newlib)
// para sus propios setjmp/longjmp de i386 -- no es una invencion nueva,
// es la implementacion de referencia trasladada a este proyecto porque
// este toolchain no trae la suya.
//
// Si en el futuro BareKernel corre en otra arquitectura (ver
// docs/kernel/kernel.md Fase 5/6 del PAL, Linux/macOS backends), esto
// necesita su propio bloque #elif defined(__x86_64__) / __aarch64__ con
// el set de registros correcto para esa ABI -- avisa con #error en vez de
// fallar en silencio si el target no esta cubierto.

#if defined(__i386__)

namespace avastd {
// ebx, esi, edi, ebp, esp, eip -- en ese orden, ver ava_error.cpp.
using ava_jmp_buf = avastd::uint32_t[6];
}

extern "C" {
__attribute__((naked)) int ava_setjmp(avastd::uint32_t* buf);
__attribute__((naked)) void ava_longjmp(avastd::uint32_t* buf, int val);
}

#define setjmp(buf) ava_setjmp(buf)
#define longjmp(buf, val) ava_longjmp(buf, val)

#else
  #error "avastd error handling (setjmp/longjmp propio) solo tiene implementacion i386 hoy. Agregar el bloque para esta arquitectura en ava_error.h antes de continuar (ver comentario arriba)."
#endif

namespace avastd { using jmp_buf = ava_jmp_buf; }

namespace avastd {

// Base minima que reemplaza std::exception -- solo lo que AvaLang usa
// (what()).
class exception {
public:
    exception() = default;
    explicit exception(const string& msg) : msg_(msg) {}
    virtual ~exception() = default;
    virtual const char* what() const noexcept { return msg_.c_str(); }

    // Sin RTTI real (ver nota grande arriba), un solo AVA_CATCH no puede
    // discriminar "cual tipo exacto fue lanzado" como catch(TipoA&) vs
    // catch(TipoB&) reales harian. Para los pocos lugares de AvaLang que
    // de verdad necesitan distinguir mas de un tipo dentro del mismo
    // AVA_TRY (ver ava::AvaRaiseException en vm_internal.h vs errores
    // genericos en vm.cpp::ExecuteFrame), cada subclase override-ea esto
    // con un valor propio. 0 = "generico", cualquier avastd::runtime_error
    // sin mas. No es un reemplazo general de RTTI -- es deliberadamente
    // minimo, solo para los casos concretos que hoy lo necesitan.
    virtual int ava_type_tag() const noexcept { return 0; }

protected:
    string msg_;
};

class runtime_error : public exception {
public:
    explicit runtime_error(const string& msg) : exception(msg) {}
    explicit runtime_error(const char* msg) : exception(string(msg)) {}
};

// Pila de manejadores. Modelada como pila (no una sola global) porque el
// VM tiene coroutines cooperativas (ver src/vm/coroutine.cpp en tu
// historial) que pueden anidar/reentrar AVA_TRY al suspenderse y
// reanudarse -- igual que ya haces con pending_finally_stack_ en el
// Compiler para 'finally' dentro de try/catch anidados.
struct ErrorFrame {
    jmp_buf buf;
    exception* caught = nullptr;
    ErrorFrame* prev = nullptr;
};

// Definidas en ava_error.cpp para que el estado (puntero al frame actual)
// no se duplique por unidad de traduccion.
ErrorFrame*& CurrentErrorFrame();
[[noreturn]] void AvaThrow(exception* e);
// Se dispara si AVA_THROW ocurre sin ningun AVA_TRY activo (error no
// atrapable). Por defecto entra en panic-loop (igual que __cxa_throw del
// kernel hoy, ver cxx_runtime.cpp) -- el BareKernelPlatform puede
// reemplazar esto por su propia rutina de panic si hace falta reportar
// mas contexto antes de detenerse.
[[noreturn]] void AvaFatalAbort(const char* what);

namespace detail {
inline ErrorFrame* PushErrorFrame(ErrorFrame& frame) {
    frame.caught = nullptr;
    frame.prev = CurrentErrorFrame();
    CurrentErrorFrame() = &frame;
    return &frame;
}
inline avastd::nullptr_t PopErrorFrame(ErrorFrame& frame) {
    CurrentErrorFrame() = frame.prev;
    return nullptr;
}
}  // namespace detail

}  // namespace avastd

#define AVA_TRY \
    for (avastd::ErrorFrame _ava_frame, *_ava_run = avastd::detail::PushErrorFrame(_ava_frame); \
         _ava_run; _ava_run = avastd::detail::PopErrorFrame(_ava_frame)) \
        if (setjmp(_ava_frame.buf) == 0)

#define AVA_CATCH(Type, name) \
        else \
            for (Type& name = *static_cast<Type*>(_ava_frame.caught); \
                 _ava_frame.caught; _ava_frame.caught = nullptr)

#define AVA_THROW(expr) \
    avastd::AvaThrow(new avastd::remove_reference_t<decltype(expr)>(expr))

// AVA_RETHROW(): equivalente a `throw;` dentro de un catch -- repropaga
// el MISMO objeto ya atrapado (no fabrica uno nuevo, no pierde el tipo
// dinamico real que tenia, aunque no podamos consultarlo via RTTI) hacia
// el AVA_TRY que envuelve a este. Solo valido dentro de un bloque
// AVA_CATCH (igual que `throw;` real solo vale dentro de un catch).
//
// OJO al mecanismo: hay que sacar _ava_frame de la pila de frames ANTES
// de relanzar. Si no, AvaThrow ve CurrentErrorFrame() == &_ava_frame (el
// frame en el que estamos parados ahora mismo) y el longjmp nos manda de
// vuelta a nuestro propio AVA_CATCH en vez de propagar hacia afuera --
// bucle infinito. PopErrorFrame es seguro de llamar aca aunque el for-loop
// de AVA_TRY tambien lo llame en su propia iteracion: AvaThrow no
// retorna nunca (longjmp), asi que ese segundo pop jamas se ejecuta.
#define AVA_RETHROW() \
    do { avastd::detail::PopErrorFrame(_ava_frame); avastd::AvaThrow(_ava_frame.caught); } while (0)

#endif  // AVA_HAVE_EXCEPTIONS

#endif  // AVA_STDCOMPAT_ERROR_H
