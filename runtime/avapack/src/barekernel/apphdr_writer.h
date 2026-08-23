#ifndef AVAPACK_BAREKERNEL_APPHDR_WRITER_H
#define AVAPACK_BAREKERNEL_APPHDR_WRITER_H

// apphdr_writer -- Fase B1 de plan_avapack_barekernel.md, tabla §3:
// "ExecutableHeaderCreator.cs (AppBuilder, C#) -- Sí, como referencia de
// layout [...] Portar esa clase a C++ dentro de avapack/src/barekernel/
// apphdr_writer.cpp es casi mecánico."
//
// *** Verificado contra el ExecutableHeaderCreator.cs real (AppBuilder.zip) ***
// A diferencia del intento anterior (que asumía CRC-32 sobre header+payload
// sin tener el original a mano), este archivo replica CreateExecutableFile()
// campo por campo contra el C# real:
//   - Checksum: NO es CRC-32. Es una suma simple de bytes mod 2^32
//     (`checksum = (checksum + b) & 0xFFFFFFFF` en un foreach), y SOLO
//     sobre binaryData (el payload) -- el AppHeader nunca participa del
//     cálculo, ni siquiera con el campo Checksum en 0. Ver
//     ComputeAppHeaderChecksum() más abajo.
//   - TotalSize: es únicamente el tamaño del payload (`binaryData.Length`),
//     NO header+payload como podría sugerir el nombre.
//   - DataOffset: siempre `HEADER_SIZE + binaryData.Length` (justo después
//     del código), aunque DataSize sea 0 -- no se deja en 0 como en un
//     intento previo sin el original.
//   - StackSize/Flags: AppBuilder usa defaults fijos reales (65536 /
//     0x01) -- ver kDefaultStackSize/kAppHeaderFlags más abajo.
//   - BssSize: a diferencia de StackSize/Flags, ESTE campo depende del
//     binario concreto (cuánto .bss real tiene el ELF compilado) -- no
//     es una política fija de AppBuilder que tenga sentido "clonar" como
//     constante. Ver elf32_bss.h: se calcula leyendo la tabla de
//     secciones del .elf (suma de SHT_NOBITS+SHF_ALLOC), no se adivina.
//     (Versión anterior de este archivo tenía kDefaultBssSize=4096 como
//     fallback silencioso -- eso asumía que ninguna app barekernel tiene
//     estado global no trivial, supuesto que se rompe en cuanto alguien
//     agrega un array/struct estático a cualquier app de este target; el
//     kernel no valida bss_size contra nada real, así que un valor
//     mentiroso ahí se traduce directo en memoria de proceso
//     insuficiente y un page fault en runtime. Ver git blame / elf32_bss.h
//     para el caso real que motivó sacarlo.)
//   - EntryPoint: semántica confirmada contra EntryPointDetector.cs -- es
//     un offset dentro de binaryData (heurística de patrones de bytes en
//     AppBuilder). Este componente no reimplementa esa heurística: usa el
//     offset real de `_start` obtenido de `nm`/el linker antes de aplanar,
//     que es estrictamente más preciso que adivinar por firma de bytes.
//
// El LAYOUT del struct sigue verificado además contra docs/kernel/kernel.md
// §2.2. Recordar (tabla Fase 0 del plan): el kernel de hoy
// (`Loader::validate_header()`, src/arch/x86/process/loader.cpp) NUNCA
// verifica el campo `checksum` -- es decorativo del lado del kernel tal
// como está litekernel.zip hoy, así que un checksum incorrecto no bloquea
// correr nada en QEMU, pero sí bloqueaba (antes de este fix) que el .exe
// resultante fuera binario-compatible con lo que produce el AppBuilder
// real si algún día se comparan bit a bit.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace avapack_barekernel {

// Magic "EXEC" en little-endian, como está documentado en kernel.md §2.2:
// los 4 bytes en el archivo son 'E' 'X' 'E' 'C' -- 0x43455845 es ese
// mismo valor leído como uint32_t little-endian (0x45 'E' es el byte
// menos significativo).
constexpr std::uint32_t kAppHeaderMagic = 0x43455845u;
constexpr std::uint32_t kAppHeaderVersion = 1u;

// Defaults reales de AppBuilder (ExecutableHeaderCreator.cs,
// CreateExecutableFile()) -- no eran parámetros libres, así que se
// replican acá como constantes en vez de dejarlos "a criterio del
// llamador". Sobreescribibles si algún día hace falta (ver
// FlatBinaryLayout/WriteAppHeaderWrapped), pero este es el valor que usa
// AppBuilder para todas sus apps C++.
constexpr std::uint32_t kDefaultStackSize = 65536u;  // 64KB
constexpr std::uint32_t kAppHeaderFlags = 0x01u;     // "Executable", único valor que usa AppBuilder
// (Ya no hay kDefaultBssSize acá -- ver comentario de cabecera arriba y
// elf32_bss.h. bss_size se calcula por binario, nunca se hardcodea.)

// Layout verificado contra docs/kernel/kernel.md §2.2, campo por campo.
// `__attribute__((packed))` en el header de kernel.md implica sin padding
// entre miembros -- replicado acá con #pragma pack para que sizeof() dé
// exactamente 64 bytes (12 campos escalares + reserved[4], todos
// uint32_t) sin depender de que el compilador de host use la misma
// convención de alineación que i686-elf-g++.
#pragma pack(push, 1)
struct AppHeader {
    std::uint32_t magic;         // kAppHeaderMagic
    std::uint32_t version;       // kAppHeaderVersion
    std::uint32_t entry_point;   // offset desde la base de carga (0x40000000), NO address absoluta
    std::uint32_t total_size;    // tamaño total del .exe incluyendo este header
    std::uint32_t code_offset;   // offset del segmento de código dentro del archivo
    std::uint32_t code_size;
    std::uint32_t data_offset;   // offset del segmento de datos dentro del archivo
    std::uint32_t data_size;
    std::uint32_t bss_size;      // no ocupa espacio en el archivo, solo se reserva en memoria
    std::uint32_t stack_size;
    std::uint32_t flags;         // sin uso conocido documentado todavía -- se deja en 0
    std::uint32_t checksum;      // ver advertencia de cabecera: algoritmo no verificado contra
                                  // ExecutableHeaderCreator.cs real, y el kernel hoy no lo valida
    std::uint32_t reserved[4];   // sin uso documentado -- se deja en 0
};
#pragma pack(pop)

static_assert(sizeof(AppHeader) == 64,
              "AppHeader debe medir 64 bytes (12 campos escalares + reserved[4], todos uint32_t) "
              "-- ver kernel.md §2.2");

// Parámetros de un layout de app de un solo segmento de código (sin
// segmento de datos inicializados separado) -- es el caso común para
// `objcopy -O binary` sobre un .elf freestanding chico como
// main_barekernel.cpp + el .cpp generado por avapack_barekernel_gen, y es
// lo único que este componente necesita hoy (Fase B1). Si algún target
// futuro necesita separar .data de .bss explícitamente, agregar un
// segundo constructor -- no vale la pena generalizar sin un caso real que
// lo pida (mismo criterio que el resto del plan sobre no anticipar
// trabajo, ver §2).
struct FlatBinaryLayout {
    std::uint32_t entry_offset;  // offset de _start dentro del .bin, == entry_point del header
    std::uint32_t stack_size = kDefaultStackSize;  // en bytes; default real de AppBuilder,
                                                    // sobreescribible si hace falta
};

// Calcula el checksum del payload, verificado contra
// ExecutableHeaderCreator.CalculateChecksum() real: suma simple de bytes
// mod 2^32, SOLO sobre `payload` -- el AppHeader nunca participa de este
// cálculo (a diferencia de un intento previo que asumía CRC-32 sobre
// header+payload sin tener el C# original a mano).
std::uint32_t ComputeAppHeaderChecksum(const unsigned char* payload, std::size_t payload_size);

// Envuelve `flat_bin` (el resultado de `objcopy -O binary` sobre el .elf
// de main_barekernel.cpp + embedded_avb.cpp) con un AppHeader y escribe
// el .exe final en `out_path`. `layout.entry_offset` debe ser el offset
// de `_start` dentro de `flat_bin` (se obtiene con `nm`/el propio linker
// contra el .elf antes de aplanarlo -- ver tabla del plan sobre por qué
// NO se reusa la heurística de EntryPointDetector.cs: acá se conoce con
// certeza, no hace falta adivinar por firma de bytes).
//
// Este componente asume un solo segmento (todo `flat_bin` es "código";
// data_size queda en 0, bss_size se pasa aparte porque .bss no ocupa
// espacio en el archivo). Devuelve false si no se pudo escribir el
// archivo de salida.
bool WriteAppHeaderWrapped(const std::vector<unsigned char>& flat_bin,
                            const FlatBinaryLayout& layout,
                            std::uint32_t bss_size,
                            const std::string& out_path);

} // namespace avapack_barekernel

#endif // AVAPACK_BAREKERNEL_APPHDR_WRITER_H
