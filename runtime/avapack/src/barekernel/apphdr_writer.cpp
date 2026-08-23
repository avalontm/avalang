#include "apphdr_writer.h"

#include <array>
#include <cstring>
#include <fstream>

namespace avapack_barekernel {

// Verificado contra ExecutableHeaderCreator.CalculateChecksum() real
// (AppBuilder, C#):
//   uint checksum = 0;
//   foreach (byte b in data) checksum = (checksum + b) & 0xFFFFFFFF;
//   return checksum;
// Es decir: suma simple de bytes mod 2^32, SOLO sobre `payload` -- el
// AppHeader no participa de este cálculo en absoluto (ni con el campo
// Checksum en 0, como sí asumía CRC-32-sobre-header+payload en un intento
// previo sin el original a mano). std::uint32_t ya trunca a 32 bits en
// cada suma, así que el `& 0xFFFFFFFF` del C# es implícito acá.
std::uint32_t ComputeAppHeaderChecksum(const unsigned char* payload, std::size_t payload_size) {
    std::uint32_t checksum = 0;
    for (std::size_t i = 0; i < payload_size; ++i) {
        checksum = checksum + payload[i];
    }
    return checksum;
}

bool WriteAppHeaderWrapped(const std::vector<unsigned char>& flat_bin,
                            const FlatBinaryLayout& layout,
                            std::uint32_t bss_size,
                            const std::string& out_path) {
    if (flat_bin.empty()) {
        return false; // nada que envolver -- probablemente objcopy fallo antes de llegar acá
    }

    // Campo por campo contra ExecutableHeaderCreator.CreateExecutableFile()
    // real (AppBuilder.zip) -- ver advertencia de apphdr_writer.h para el
    // detalle de qué cambió respecto a un intento previo sin el original.
    AppHeader header{};
    header.magic = kAppHeaderMagic;
    header.version = kAppHeaderVersion;
    header.entry_point = layout.entry_offset;
    header.total_size = static_cast<std::uint32_t>(flat_bin.size()); // solo el payload, NO header+payload
    header.code_offset = static_cast<std::uint32_t>(sizeof(AppHeader));
    header.code_size = static_cast<std::uint32_t>(flat_bin.size());
    // justo despues del codigo, aunque data_size sea 0
    header.data_offset = static_cast<std::uint32_t>(sizeof(AppHeader) + flat_bin.size());
    header.data_size = 0;
    header.bss_size = bss_size;
    header.stack_size = layout.stack_size;
    header.flags = kAppHeaderFlags; // 0x01, "Executable" -- unico valor que usa AppBuilder
    // solo el payload -- el header NUNCA participa del checksum
    header.checksum = ComputeAppHeaderChecksum(flat_bin.data(), flat_bin.size());
    for (auto& r : header.reserved) r = 0;

    std::ofstream out(out_path, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(flat_bin.data()),
              static_cast<std::streamsize>(flat_bin.size()));

    return out.good();
}

} // namespace avapack_barekernel
