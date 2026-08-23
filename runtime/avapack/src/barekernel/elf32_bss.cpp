#include "elf32_bss.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace avapack_barekernel {

namespace {

// Subconjunto de ELF32 (formato de i686-elf-ld) que necesitamos: header
// + section header table. Layout verificado contra el System V ABI /
// ELF32 spec (Chapter 4/5, "e_shoff"/"Section Header"). Todo little
// endian -- es la única endianness que produce el toolchain i686-elf de
// este proyecto (ver cmake/toolchain-i686-elf.cmake), así que no hace
// falta manejar big endian.

constexpr unsigned char kElfMagic[4] = {0x7f, 'E', 'L', 'F'};
constexpr unsigned char kElfClass32 = 1;
constexpr unsigned char kElfDataLsb = 1;  // little endian

constexpr std::uint32_t kShtNobits = 8;
constexpr std::uint32_t kShfAlloc = 0x2;

#pragma pack(push, 1)
struct Elf32Header {
    unsigned char e_ident[16];
    std::uint16_t e_type;
    std::uint16_t e_machine;
    std::uint32_t e_version;
    std::uint32_t e_entry;
    std::uint32_t e_phoff;
    std::uint32_t e_shoff;
    std::uint32_t e_flags;
    std::uint16_t e_ehsize;
    std::uint16_t e_phentsize;
    std::uint16_t e_phnum;
    std::uint16_t e_shentsize;
    std::uint16_t e_shnum;
    std::uint16_t e_shstrndx;
};

struct Elf32SectionHeader {
    std::uint32_t sh_name;
    std::uint32_t sh_type;
    std::uint32_t sh_flags;
    std::uint32_t sh_addr;
    std::uint32_t sh_offset;
    std::uint32_t sh_size;
    std::uint32_t sh_link;
    std::uint32_t sh_info;
    std::uint32_t sh_addralign;
    std::uint32_t sh_entsize;
};
#pragma pack(pop)

static_assert(sizeof(Elf32Header) == 52, "ELF32 header debe medir 52 bytes");
static_assert(sizeof(Elf32SectionHeader) == 40, "ELF32 section header debe medir 40 bytes");

} // namespace

Elf32BssResult ComputeBssSizeFromElf(const std::string& elf_path) {
    Elf32BssResult result;

    std::ifstream in(elf_path, std::ios::binary);
    if (!in) {
        result.error = "no se pudo abrir '" + elf_path + "'";
        return result;
    }

    Elf32Header header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        result.error = "'" + elf_path + "' es mas chico que un header ELF32 -- no es un ELF valido";
        return result;
    }

    if (std::memcmp(header.e_ident, kElfMagic, sizeof(kElfMagic)) != 0) {
        result.error = "'" + elf_path + "' no tiene magic ELF (0x7f 'E' 'L' 'F')";
        return result;
    }
    if (header.e_ident[4] != kElfClass32) {
        result.error = "'" + elf_path + "' no es ELFCLASS32 -- este target (i686) solo produce ELF32";
        return result;
    }
    if (header.e_ident[5] != kElfDataLsb) {
        result.error = "'" + elf_path + "' no es little-endian -- inesperado para i686-elf";
        return result;
    }
    if (header.e_shentsize != sizeof(Elf32SectionHeader)) {
        result.error = "'" + elf_path + "': e_shentsize (" + std::to_string(header.e_shentsize) +
                        ") no coincide con sizeof(Elf32SectionHeader) (" +
                        std::to_string(sizeof(Elf32SectionHeader)) + ")";
        return result;
    }
    if (header.e_shnum == 0 || header.e_shoff == 0) {
        // ELF valido pero sin tabla de secciones (p.ej. stripped a full
        // binary sin -g). No hay de donde sacar bss real.
        result.error = "'" + elf_path + "' no tiene tabla de secciones (e_shnum=0) -- "
                        "no se puede calcular bss real; volver a linkear sin strippear secciones "
                        "(el .bin final aplanado con objcopy -O binary no se ve afectado, "
                        "esto solo lee el .elf intermedio antes de aplanar)";
        return result;
    }

    std::vector<Elf32SectionHeader> sections(header.e_shnum);
    in.seekg(header.e_shoff, std::ios::beg);
    if (!in) {
        result.error = "'" + elf_path + "': e_shoff (" + std::to_string(header.e_shoff) +
                        ") fuera de rango";
        return result;
    }
    in.read(reinterpret_cast<char*>(sections.data()),
            static_cast<std::streamsize>(sizeof(Elf32SectionHeader) * sections.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(Elf32SectionHeader) * sections.size())) {
        result.error = "'" + elf_path + "': no se pudo leer la tabla de secciones completa "
                        "(archivo truncado?)";
        return result;
    }

    std::uint64_t total_bss = 0;  // 64 bits en el acumulador para detectar overflow antes de truncar
    for (const auto& sh : sections) {
        if (sh.sh_type == kShtNobits && (sh.sh_flags & kShfAlloc) != 0) {
            total_bss += sh.sh_size;
        }
    }

    if (total_bss > 0xFFFFFFFFull) {
        result.error = "'" + elf_path + "': bss total calculado (" + std::to_string(total_bss) +
                        " bytes) excede 32 bits -- header->bss_size (AppHeader, uint32_t) no puede "
                        "representarlo";
        return result;
    }

    result.bss_size = static_cast<std::uint32_t>(total_bss);
    return result;
}

} // namespace avapack_barekernel
