#ifndef AVAPACK_BAREKERNEL_ELF32_BSS_H
#define AVAPACK_BAREKERNEL_ELF32_BSS_H

// elf32_bss -- calcula el tamaño real de .bss de un ELF32 leyendo su
// tabla de secciones directamente (sin depender de `size`/`readelf`, que
// no están garantizados en un toolchain i686-elf --without-headers como
// el que usa este proyecto -- ver kb run / build_barekernel_app.bat, que
// solo verifican gcc/g++/nasm/ld).
//
// Por qué existe: antes de esto, ava_apphdr_writer usaba un
// `--bss-size` opcional con default fijo en 4096 (ver apphdr_writer.h,
// kDefaultBssSize) bajo el supuesto de que ninguna app barekernel tiene
// "estado global no trivial". Ese supuesto es frágil por construcción:
// se rompe en cuanto alguien agrega UN array/struct estático o global a
// cualquier app de este target, y el kernel (litekernel/src/arch/x86/
// process/loader.cpp, Loader::load) reserva memoria del proceso
// ÚNICAMENTE a partir de header->bss_size -- no valida contra nada real.
// Si el header miente, el proceso corre con menos memoria de la que
// necesita y accede memoria fuera de lo que se le mapeó (manifestándose
// como un page fault, típicamente contra las páginas de kernel que
// PageTable::MapKernelSpace copia a todo proceso -- present pero
// supervisor-only, exactamente el patrón de error code 5 = present+user
// +read).
//
// La definición de "bss" usada acá es la estándar de binutils `size`:
// suma de sh_size de toda sección con sh_type==SHT_NOBITS Y
// sh_flags&SHF_ALLOC. Es una cota razonable (no exacta byte a byte si el
// linker inserta padding de alineación entre secciones NOBITS, pero el
// loader ya redondea a páginas completas y agrega 2 páginas de margen --
// ver Loader::load, `total_memory += Allocator::PAGE_SIZE * 2`), y es el
// mismo criterio que cualquier herramienta estándar de binutils reporta
// como "bss" para este mismo ELF.

#include <cstdint>
#include <optional>
#include <string>

namespace avapack_barekernel {

struct Elf32BssResult {
    std::uint32_t bss_size = 0;
    std::string error;  // vacío si ok
};

// Lee `elf_path` como ELF32 little-endian y devuelve la suma de sh_size
// de sus secciones SHT_NOBITS+SHF_ALLOC. Si el archivo no es un ELF32 LE
// válido, o hay un error de lectura, `error` queda seteado (no vacío) y
// `bss_size` no debe usarse.
Elf32BssResult ComputeBssSizeFromElf(const std::string& elf_path);

} // namespace avapack_barekernel

#endif // AVAPACK_BAREKERNEL_ELF32_BSS_H
