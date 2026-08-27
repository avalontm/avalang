#ifndef AVAPACK_PAYLOAD_FORMAT_H
#define AVAPACK_PAYLOAD_FORMAT_H

// Fase 9 -- formato binario apendeado al final del stub precompilado
// (avapack_stub.exe), como alternativa a generar+compilar embedded_project.cpp
// (Fases 1-8). Ver runtime/avapack/README.md, seccion Fase 9, para el porque
// (ava_cli build --target desktop dejaba de funcionar sin el repo/CMake
// completo al lado, porque avapack_gen no generaba el .cpp -- recompilaba
// avapack desde fuente en build_pack en TODOS los casos).
//
// Contrato: avapack_gen (host, ver src/generator/main.cpp) escribe el BLOB
// con EncodePayloadBlob a un archivo suelto (--payload-out). `ava_cli build`
// (runtime/avacli/src/build_command.cpp) despues:
//   1) copia avapack_stub.exe (prebuilt, sin ningun proyecto embebido) -> --out
//   2) apendea el blob al final de esa copia
//   3) apendea un PayloadFooter de tamaño FIJO (kFooterSize bytes) al final
//      de TODO -- stub_main.cpp (compilado en avapack_stub.exe) busca ese
//      footer leyendo los ultimos kFooterSize bytes de su propio ejecutable
//      (GetModuleFileNameA/self-exe + fseek al final), valida el magic, y
//      con blob_offset/blob_size sabe exactamente que rango del archivo leer
//      para reconstruir el mismo avapack::EmbeddedFile[] que antes vivia
//      como constantes compiladas (ver embedded_project.h/embedded_crypto.h).
//
// Nada de esto cambia el algoritmo de cifrado/HMAC en si (sigue siendo
// AES-256-CTR + HMAC-SHA256 tal cual Fase 3/5) -- solo CÓMO ese contenido ya
// cifrado llega al binario final: apendeado a un .exe generico en vez de
// convertido a un array C++ que un compilador tiene que volver a traducir a
// maquina cada vez.
//
// Todos los enteros son little-endian (host x86/x64 en ambos extremos --
// avapack_gen corre en la misma maquina/arquitectura que despues ejecuta el
// .exe empacado, no hay caso de uso cross-endian conocido para este
// empacador). No hay padding implicito: los campos se leen/escriben byte a
// byte en el orden declarado, no con structs empaquetados por el compilador
// (evita depender de #pragma pack / __attribute__((packed)) siendo
// consistente entre compiladores).

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "embedded_project.h" // avapack::EmbeddedFile

namespace avapack {

// --- Footer fijo, siempre los ultimos kFooterSize bytes del .exe final ---

constexpr char kFooterMagic[8] = {'A', 'V', 'A', 'P', 'K', 'F', 'T', '1'};
constexpr std::uint32_t kFooterVersion = 1;
constexpr std::size_t kFooterSize = 8 /*magic*/ + 4 /*version*/ + 4 /*flags*/ +
                                     8 /*blob_offset*/ + 8 /*blob_size*/; // 32 bytes

struct PayloadFooter {
    char magic[8];
    std::uint32_t version = kFooterVersion;
    std::uint32_t flags = 0; // reservado, siempre 0 por ahora
    std::uint64_t blob_offset = 0; // offset absoluto, dentro del .exe, donde arranca el blob
    std::uint64_t blob_size = 0;   // tamaño del blob en bytes (sin contar este footer)
};

inline void AppendU32(std::vector<unsigned char>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
inline void AppendU64(std::vector<unsigned char>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
inline void AppendBytes(std::vector<unsigned char>& out, const unsigned char* data,
                         std::size_t len) {
    out.insert(out.end(), data, data + len);
}
inline void AppendString(std::vector<unsigned char>& out, const std::string& s) {
    AppendU32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

inline std::vector<unsigned char> EncodeFooter(const PayloadFooter& f) {
    std::vector<unsigned char> out;
    out.reserve(kFooterSize);
    out.insert(out.end(), f.magic, f.magic + 8);
    AppendU32(out, f.version);
    AppendU32(out, f.flags);
    AppendU64(out, f.blob_offset);
    AppendU64(out, f.blob_size);
    return out;
}

// Un archivo embebido, tal cual lo describe el blob -- version "codificable"
// de EmbeddedFile (que usa punteros crudos pensados para apuntar a arrays
// const compilados, no para (de)serializar).
struct PayloadFileEntry {
    std::string path;
    unsigned char nonce[16];
    std::vector<unsigned char> cipher;
};

struct PayloadBlob {
    std::uint32_t key_seed = 0;
    unsigned char key_fragment_a[16] = {};
    unsigned char key_fragment_b[16] = {};
    unsigned char integrity_mac[32] = {};
    bool debug_build = false;
    bool entry_is_bytecode = false;
    bool entry_strings_obfuscated = false;
    std::uint64_t entry_obfuscate_seed = 0;
    std::string entry_file;
    std::vector<PayloadFileEntry> files;
};

// Usado por avapack_gen (host, --payload-out) -- serializa todo lo que hoy
// se emite como C++ (ver generator/main.cpp) a este formato binario en vez
// de a un .cpp.
inline std::vector<unsigned char> EncodePayloadBlob(const PayloadBlob& blob) {
    std::vector<unsigned char> out;
    AppendU32(out, blob.key_seed);
    AppendBytes(out, blob.key_fragment_a, 16);
    AppendBytes(out, blob.key_fragment_b, 16);
    AppendBytes(out, blob.integrity_mac, 32);
    out.push_back(blob.debug_build ? 1 : 0);
    out.push_back(blob.entry_is_bytecode ? 1 : 0);
    out.push_back(blob.entry_strings_obfuscated ? 1 : 0);
    out.push_back(0); // reservado
    AppendU64(out, blob.entry_obfuscate_seed);
    AppendString(out, blob.entry_file);
    AppendU32(out, static_cast<std::uint32_t>(blob.files.size()));
    for (const auto& f : blob.files) {
        AppendString(out, f.path);
        AppendBytes(out, f.nonce, 16);
        AppendU64(out, static_cast<std::uint64_t>(f.cipher.size()));
        AppendBytes(out, f.cipher.data(), f.cipher.size());
    }
    return out;
}

// --- Lado de lectura (stub_main.cpp) ---

class PayloadReader {
public:
    PayloadReader(const unsigned char* data, std::size_t size) : data_(data), size_(size) {}

    bool ReadU32(std::uint32_t& out) {
        if (pos_ + 4 > size_) return false;
        out = static_cast<std::uint32_t>(data_[pos_]) |
              (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
              (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
              (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return true;
    }
    bool ReadU64(std::uint64_t& out) {
        if (pos_ + 8 > size_) return false;
        out = 0;
        for (int i = 0; i < 8; ++i) {
            out |= static_cast<std::uint64_t>(data_[pos_ + i]) << (8 * i);
        }
        pos_ += 8;
        return true;
    }
    bool ReadBytes(unsigned char* out, std::size_t len) {
        if (pos_ + len > size_) return false;
        std::memcpy(out, data_ + pos_, len);
        pos_ += len;
        return true;
    }
    bool ReadString(std::string& out) {
        std::uint32_t len = 0;
        if (!ReadU32(len)) return false;
        if (pos_ + len > size_) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return true;
    }
    bool ReadVector(std::vector<unsigned char>& out, std::size_t len) {
        if (pos_ + len > size_) return false;
        out.assign(data_ + pos_, data_ + pos_ + len);
        pos_ += len;
        return true;
    }

private:
    const unsigned char* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
};

// Decodifica el footer desde los ultimos kFooterSize bytes de un buffer
// (stub_main.cpp los lee del final de su propio ejecutable). Devuelve false
// si el magic/tamaño no coinciden -- eso indica un avapack_stub.exe sin
// payload apendeado (build incompleto/corrupto), no un proyecto valido.
inline bool DecodeFooter(const unsigned char* tail, std::size_t tail_len, PayloadFooter& out) {
    if (tail_len < kFooterSize) return false;
    const unsigned char* p = tail + (tail_len - kFooterSize);
    if (std::memcmp(p, kFooterMagic, 8) != 0) return false;
    PayloadReader r(p + 8, kFooterSize - 8);
    if (!r.ReadU32(out.version)) return false;
    if (!r.ReadU32(out.flags)) return false;
    if (!r.ReadU64(out.blob_offset)) return false;
    if (!r.ReadU64(out.blob_size)) return false;
    std::memcpy(out.magic, kFooterMagic, 8);
    return out.version == kFooterVersion;
}

// Decodifica un blob ya leido a memoria (ver DecodeFooter para ubicarlo
// dentro del .exe). false si esta truncado/corrupto -- el llamador debe
// tratarlo como "binario invalido", igual que un HMAC que no matchea.
inline bool DecodePayloadBlob(const unsigned char* data, std::size_t size, PayloadBlob& out) {
    PayloadReader r(data, size);
    if (!r.ReadU32(out.key_seed)) return false;
    if (!r.ReadBytes(out.key_fragment_a, 16)) return false;
    if (!r.ReadBytes(out.key_fragment_b, 16)) return false;
    if (!r.ReadBytes(out.integrity_mac, 32)) return false;
    unsigned char flags[4];
    if (!r.ReadBytes(flags, 4)) return false;
    out.debug_build = flags[0] != 0;
    out.entry_is_bytecode = flags[1] != 0;
    out.entry_strings_obfuscated = flags[2] != 0;
    if (!r.ReadU64(out.entry_obfuscate_seed)) return false;
    if (!r.ReadString(out.entry_file)) return false;
    std::uint32_t file_count = 0;
    if (!r.ReadU32(file_count)) return false;
    out.files.clear();
    out.files.reserve(file_count);
    for (std::uint32_t i = 0; i < file_count; ++i) {
        PayloadFileEntry entry;
        if (!r.ReadString(entry.path)) return false;
        if (!r.ReadBytes(entry.nonce, 16)) return false;
        std::uint64_t content_len = 0;
        if (!r.ReadU64(content_len)) return false;
        if (!r.ReadVector(entry.cipher, static_cast<std::size_t>(content_len))) return false;
        out.files.push_back(std::move(entry));
    }
    return true;
}

// Construye el array avapack::EmbeddedFile[] (punteros crudos) que el resto
// del runtime empacado espera (embedded_crypto.h), apuntando DENTRO de
// `blob` -- `blob` tiene que seguir viva mientras se use el resultado (los
// punteros de EmbeddedFile no son owning).
inline std::vector<EmbeddedFile> BuildEmbeddedFilesView(const PayloadBlob& blob) {
    std::vector<EmbeddedFile> files;
    files.reserve(blob.files.size());
    for (const auto& f : blob.files) {
        EmbeddedFile ef;
        ef.path = f.path.c_str();
        ef.cipher = f.cipher.empty() ? nullptr : f.cipher.data();
        ef.content_len = f.cipher.size();
        ef.nonce = f.nonce;
        files.push_back(ef);
    }
    return files;
}

} // namespace avapack

#endif // AVAPACK_PAYLOAD_FORMAT_H
