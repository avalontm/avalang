#include "ava_error.h"
#include "ava_platform_caps.h"

#if !AVA_HAVE_EXCEPTIONS

#if defined(__i386__)

// Implementacion propia de setjmp/longjmp para i386 (cdecl). Ver el
// comentario largo en ava_error.h para el porque (setjmp.h es de libc,
// este toolchain no tiene libc). Layout del buffer: ebx, esi, edi, ebp,
// esp, eip -- los 5 registros callee-saved de cdecl mas la direccion de
// retorno. "naked" = sin prologo/epilogo generado por el compilador, asi
// el asm tiene control total sobre la pila (imprescindible: si el
// compilador insertara su propio push/pop de %ebp antes de que corra nuestro
// asm, los offsets 4(%esp)/(%esp) para leer los argumentos ya no serian
// correctos).
//
// Validado con un test standalone (no solo "compila"): gcc -m32 en
// -O0/-O2/-O3 -fomit-frame-pointer, setjmp en main(), 6 niveles de
// recursion quemando stack a proposito, longjmp de vuelta -- confirma
// valor de retorno y variables locales previas al setjmp intactos.

extern "C" __attribute__((naked)) int ava_setjmp(avastd::uint32_t*) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n"
        "movl %ebx, 0(%eax)\n"
        "movl %esi, 4(%eax)\n"
        "movl %edi, 8(%eax)\n"
        "movl %ebp, 12(%eax)\n"
        "leal 4(%esp), %ecx\n"      // esp que vera el caller justo despues de este ret
        "movl %ecx, 16(%eax)\n"
        "movl (%esp), %ecx\n"       // direccion de retorno == eip a donde volver
        "movl %ecx, 20(%eax)\n"
        "xorl %eax, %eax\n"         // primera pasada: setjmp devuelve 0
        "ret\n"
    );
}

extern "C" __attribute__((naked)) void ava_longjmp(avastd::uint32_t*, int) {
    __asm__ volatile(
        "movl 4(%esp), %edx\n"      // edx = buf
        "movl 8(%esp), %ecx\n"      // ecx = val
        "testl %ecx, %ecx\n"
        "jnz 1f\n"
        "movl $1, %ecx\n"           // longjmp(buf, 0) -> setjmp "devuelve" 1, no 0
        "1:\n"
        "movl 0(%edx), %ebx\n"
        "movl 4(%edx), %esi\n"
        "movl 8(%edx), %edi\n"
        "movl 12(%edx), %ebp\n"
        "movl 16(%edx), %esp\n"     // stack restaurado -- de aca en mas, nada de direcciones relativas a esp del longjmp mismo
        "movl 20(%edx), %edx\n"     // edx = eip destino (edx ya no hace falta como buf)
        "movl %ecx, %eax\n"         // eax = valor de retorno visible en el setjmp original
        "jmp *%edx\n"
    );
}

#else
  #error "ava_setjmp/ava_longjmp solo implementados para i386 -- ver ava_error.h"
#endif  // defined(__i386__)

namespace avastd {

ErrorFrame*& CurrentErrorFrame() {
    // Un solo puntero global porque CKM_CAP_THREADS=0 en este kernel: no
    // hay ejecucion paralela real que pise este estado. Si Fase 4 activa
    // threads, esto pasa a thread_local (soporte de lenguaje, no de
    // libreria -- el compilador lo resuelve sin libstdc++).
    static ErrorFrame* current = nullptr;
    return current;
}

[[noreturn]] void AvaThrow(exception* e) {
    ErrorFrame* frame = CurrentErrorFrame();
    if (!frame) {
        AvaFatalAbort(e->what());
    }
    frame->caught = e;
    longjmp(frame->buf, 1);
    __builtin_unreachable();  // longjmp no retorna nunca; ayuda al analisis del compilador
}

[[noreturn]] void AvaFatalAbort(const char* what) {
    // Punto de enganche unico para reportar el error antes de detenerse.
    // Hoy: mismo comportamiento que __cxa_throw en corlib/src/cxx_runtime.cpp
    // (panic-loop) para no introducir un segundo mecanismo de parada
    // distinto del que ya usa el resto del kernel ante un fallo
    // irrecuperable. Dejado como panic-loop simple a proposito en Fase 0
    // -- ver nota en ava_error.h sobre no crear una dependencia circular
    // con las clases PAL concretas de platform/barekernel/.
    (void)what;
    for (;;) {
        // panic-loop intencional -- ver nota arriba.
    }
}

}  // namespace avastd

#endif  // !AVA_HAVE_EXCEPTIONS
