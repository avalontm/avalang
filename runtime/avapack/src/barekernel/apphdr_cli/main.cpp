// ava_apphdr_writer -- CLI de host para apphdr_writer.{h,cpp} (Fase B1 de
// plan_avapack_barekernel.md). Toma el .bin plano que deja
// `objcopy -O binary` sobre el .elf de main_barekernel.cpp +
// embedded_avb.cpp (compilado con el toolchain i686-elf) y le antepone el
// AppHeader -- ver apphdr_writer.h para el layout y las advertencias sobre
// el algoritmo de checksum (no verificado contra ExecutableHeaderCreator.cs
// real, que no está en este repositorio).
//
// Uso:
//   ava_apphdr_writer --bin <app.bin> --entry-offset <N> --elf <app.elf>
//                      [--stack-size <N>] [--bss-size <N>] --out <app.exe>
//
// --entry-offset: offset de _start dentro de --bin. Se obtiene con
//   `i686-elf-nm app.elf | grep ' _start$'` (o el propio linker) ANTES de
//   aplanar con objcopy -- ver tabla del plan sobre por qué no se
//   reimplementa la heurística de EntryPointDetector.cs de AppBuilder.
// --stack-size: tamaño de stack a reservar para el proceso, en bytes.
//   Default: 65536 (AppBuilder real default) -- esto SÍ es una política
//   fija razonable como default, no depende del binario.
// --elf: ruta al .elf SIN aplanar (el mismo que se le pasa a
//   i686-elf-nm para sacar --entry-offset), usado para calcular el
//   bss_size real del binario leyendo su tabla de secciones (ver
//   elf32_bss.h: suma de secciones SHT_NOBITS+SHF_ALLOC -- la misma
//   definición que usa `size` de binutils). Requerido salvo que se pase
//   --bss-size explícito.
// --bss-size: override manual del bss_size calculado desde --elf. Normalmente
//   NO hace falta -- solo para casos raros donde el auto-cálculo no aplica
//   (p.ej. estado que el proceso necesita en runtime pero que no vive en
//   ninguna sección SHF_ALLOC del binario). Si se pasa junto con --elf, gana
//   --bss-size y se imprime una advertencia (queda bajo tu responsabilidad
//   que el valor sea suficiente).
//
// Nota histórica: antes este flag tenía un default fijo en 4096 bytes
// (el "default real de AppBuilder"), que se usaba silenciosamente si no
// se pasaba nada. Eso asumía que ninguna app de este target iba a tener
// jamás estado global/estático no trivial -- supuesto que ya no se
// sostiene en general y que, al romperse, no daba ningún error en build
// time: el .exe se generaba "bien" pero corría con memoria insuficiente
// y crasheaba en QEMU con un page fault difícil de rastrear hasta acá.
// Por eso ahora --elf (auto-cálculo) es el camino por default y ya no
// hay ningún número mágico hardcodeado para bss_size en este archivo.

#include "../elf32_bss.h"
#include "../apphdr_writer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string bin_path;
    std::string out_path;
    std::string elf_path;
    std::optional<std::uint32_t> entry_offset;
    // Stack: default real de AppBuilder (ver apphdr_writer.h), es una
    // política fija razonable. Sobreescribible con --stack-size.
    std::uint32_t stack_size = avapack_barekernel::kDefaultStackSize;
    // Bss: SIN default hardcodeado -- se calcula desde --elf, o el
    // usuario lo pasa explícito con --bss-size. Ver comentario de
    // cabecera de este archivo.
    std::optional<std::uint32_t> bss_size;
};

void PrintUsage() {
    std::cerr << "uso: ava_apphdr_writer --bin <app.bin> --entry-offset <N> --elf <app.elf>\n"
                 "                        [--stack-size <N>] [--bss-size <N>] --out <app.exe>\n"
                 "  --elf         .elf sin aplanar del mismo build (antes de objcopy -O binary).\n"
                 "                Se usa para calcular bss_size leyendo la tabla de secciones\n"
                 "                (suma de SHT_NOBITS+SHF_ALLOC). Requerido salvo que pases\n"
                 "                --bss-size a mano.\n"
                 "  --stack-size  default: 65536 (AppBuilder real default)\n"
                 "  --bss-size    override manual del valor auto-calculado desde --elf.\n"
                 "                Sin --elf NI --bss-size, el comando falla -- ya no hay un\n"
                 "                default de 4096 silencioso (ver comentario de cabecera).\n";
}

bool ParseU32(const std::string& s, std::uint32_t* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 0); // acepta 0x... tambien
    if (end == s.c_str() || *end != '\0') return false;
    *out = static_cast<std::uint32_t>(v);
    return true;
}

bool ParseArgs(int argc, char** argv, Options* opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string();
        };
        if (arg == "--bin") {
            opts->bin_path = next();
        } else if (arg == "--elf") {
            opts->elf_path = next();
        } else if (arg == "--out") {
            opts->out_path = next();
        } else if (arg == "--entry-offset") {
            std::uint32_t v;
            if (!ParseU32(next(), &v)) { std::cerr << "error: --entry-offset invalido\n"; return false; }
            opts->entry_offset = v;
        } else if (arg == "--stack-size") {
            std::uint32_t v;
            if (!ParseU32(next(), &v)) { std::cerr << "error: --stack-size invalido\n"; return false; }
            opts->stack_size = v;
        } else if (arg == "--bss-size") {
            std::uint32_t v;
            if (!ParseU32(next(), &v)) { std::cerr << "error: --bss-size invalido\n"; return false; }
            opts->bss_size = v;
        } else {
            std::cerr << "error: argumento desconocido: " << arg << "\n";
            return false;
        }
    }
    if (opts->bin_path.empty() || opts->out_path.empty() || !opts->entry_offset.has_value()) {
        return false;
    }
    return true;
}

bool ReadWholeFile(const std::string& path, std::vector<unsigned char>* out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    std::streamsize size = in.tellg();
    if (size < 0) return false;
    in.seekg(0, std::ios::beg);
    out->resize(static_cast<std::size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(out->data()), size)) return false;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (!ParseArgs(argc, argv, &opts)) {
        PrintUsage();
        return 1;
    }

    std::vector<unsigned char> flat_bin;
    if (!ReadWholeFile(opts.bin_path, &flat_bin)) {
        std::cerr << "error: no se pudo leer --bin '" << opts.bin_path << "'\n";
        return 1;
    }

    // Resolucion de bss_size: --bss-size explicito gana siempre (con
    // advertencia si tambien vino --elf, para que quede claro que el
    // auto-calculo se esta ignorando a proposito). Sin --bss-size, --elf
    // es obligatorio -- ya no hay fallback silencioso a un numero fijo.
    std::uint32_t resolved_bss_size = 0;
    if (opts.bss_size) {
        resolved_bss_size = *opts.bss_size;
        if (!opts.elf_path.empty()) {
            std::cerr << "aviso: --bss-size " << resolved_bss_size << " explicito pisa el "
                         "auto-calculo desde --elf '" << opts.elf_path << "' -- asegurate de que "
                         "sea >= al .bss real del binario (ver elf32_bss.h).\n";
        }
    } else if (!opts.elf_path.empty()) {
        avapack_barekernel::Elf32BssResult elf_bss = avapack_barekernel::ComputeBssSizeFromElf(opts.elf_path);
        if (!elf_bss.error.empty()) {
            std::cerr << "error: no se pudo calcular bss_size desde --elf: " << elf_bss.error << "\n"
                         "       (pasa --bss-size a mano si este .elf es un caso especial)\n";
            return 1;
        }
        resolved_bss_size = elf_bss.bss_size;
        std::cerr << "info: bss_size calculado desde '" << opts.elf_path << "': "
                  << resolved_bss_size << " bytes (suma de secciones SHT_NOBITS+SHF_ALLOC)\n";
    } else {
        std::cerr << "error: falta --elf (para calcular bss_size automaticamente) o --bss-size "
                     "(override manual) -- ver --help. Este comando ya no asume un default de "
                     "4096 bytes en silencio.\n";
        PrintUsage();
        return 1;
    }

    avapack_barekernel::FlatBinaryLayout layout{};
    layout.entry_offset = *opts.entry_offset;
    layout.stack_size = opts.stack_size;

    if (!avapack_barekernel::WriteAppHeaderWrapped(flat_bin, layout, resolved_bss_size, opts.out_path)) {
        std::cerr << "error: no se pudo escribir --out '" << opts.out_path << "'\n";
        return 1;
    }

    std::cerr << "OK: " << (flat_bin.size() + sizeof(avapack_barekernel::AppHeader))
              << " bytes escritos en '" << opts.out_path << "' (AppHeader + "
              << flat_bin.size() << " bytes de codigo, entry_offset=0x" << std::hex
              << *opts.entry_offset << std::dec << ")\n";
    return 0;
}
