#ifndef AVAPACK_BAREKERNEL_EMBEDDED_AVB_H
#define AVAPACK_BAREKERNEL_EMBEDDED_AVB_H

#if defined(AVA_BAREKERNEL_TARGET_BINDING) && defined(CKM_CAP_LIBSTDCPP) && !CKM_CAP_LIBSTDCPP
// Mismo problema y mismo fix que runtime/avalang/api/include/avalang.h
// (ver el comentario grande ahí para el razonamiento completo): este
// toolchain (i686-elf-*, --without-headers) no tiene libc, así que ni
// siquiera los headers "freestanding" de libstdc++ como <cstddef> están
// disponibles -- <cstddef> de este toolchain intenta un #include_next
// hacia una libc que no existe y falla con "No such file or directory".
// Se detectó recién al compilar de verdad contra el toolchain real (antes
// solo se había validado con -fsyntax-only en el host, que sí tiene
// libstdc++, así que este error no aparecía ahí).
//
// size_t (sin calificar) es lo único que este header necesita (kAvbBytes
// usa `unsigned char`, tipo built-in, no hace falta <cstdint> para eso).
// Deliberadamente NO se incluye stdcompat/ava_types.h para no crear una
// dependencia de ruta relativa extra -- mismo criterio que avalang.h. Y
// deliberadamente NO se reabre `namespace std` para inyectar std::size_t
// (UB per el estandar) -- en vez de eso, kAvbSize usa size_t a secas más
// abajo, igual que el resto de este target hace vía
// stdcompat/ava_types.h.
typedef __SIZE_TYPE__ size_t;
#else
#include <cstddef>
using std::size_t;
#endif

// Contrato compartido entre avapack_barekernel_gen (gen/main.cpp, produce
// un embedded_avb.cpp que define estos símbolos) y main_barekernel.cpp (lo
// consume). Ver plan_avapack_barekernel.md §3.
//
// A diferencia de avapack::EmbeddedFile (../embedded_project.h), esto NO es
// un array de N archivos ni lleva cifrado/HMAC/hooks -- ver la tabla "Qué
// SÍ reusar de avapack desktop, y qué NO" en el plan:
//   - Sin cifrado (Fase 3 del target desktop): el bloqueador real acá es
//     que el binding compile/corra contra hardware real, no que alguien
//     lea el .avb con `strings`. Agregarlo después es mecánico si hace
//     falta -- no ahora.
//   - Sin verificación de integridad HMAC (Fase 5 del target desktop):
//     mismo motivo, no hay nada que validar todavía hasta tener builds
//     reales corriendo en QEMU/hardware.
//   - Sin imports (nada de EmbeddedFile[]/kEntryFile por archivo): la
//     primera versión de este componente soporta un único entry, sin
//     `import`, embebido entero -- ver tabla del plan sobre por qué los
//     hooks SetBeforeModuleReadHook/SetAfterModuleReadHook no aplican
//     todavía (CKM_CAP_THREADS=0, CKM_CAP_TIMERS=0, y no hay filesystem
//     de host al que "materializar" un import temporalmente).
//   - Siempre bytecode precompilado (kEntryIsBytecode de Fase 6 del target
//     desktop, pero es el ÚNICO modo acá, no una opción): no existe
//     compilar .ava dentro del kernel (no hay ANTLR en el binding
//     freestanding, ver binding.cmake), así que kAvbBytes es SIEMPRE un
//     módulo .avb ya serializado con ava_module_serialize (mismo formato
//     .avbc, magic "AVBC" -- ver nota sobre la doble extensión .avb/.avbc
//     en el plan, §3).
//
// Extensión: `.avb`, para no romper la convención ya usada en
// avabare/README.md y en platform/barekernel/samples/hello.avb -- NO
// `.avbc` (ese es el nombre usado del lado avapack desktop para el mismo
// formato binario).

namespace avapack_barekernel {

// Bytes crudos del módulo .avb (formato .avbc/proto_io.h) del entry point,
// generados por ava_module_serialize en el host y embebidos tal cual por
// avapack_barekernel_gen -- sin transformar, sin cifrar.
extern const unsigned char kAvbBytes[];
extern const size_t kAvbSize;

// Nombre del entry (informativo, para mensajes de error -- p.ej.
// "app.ava" o el source_name que traía el Proto original). No se usa para
// resolver imports porque este componente todavía no los soporta.
extern const char* const kEntryName;

} // namespace avapack_barekernel

#endif // AVAPACK_BAREKERNEL_EMBEDDED_AVB_H
