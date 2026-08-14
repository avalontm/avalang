#ifndef AVAPACK_EMBEDDED_CRYPTO_H
#define AVAPACK_EMBEDDED_CRYPTO_H

// Extraido de src/main.cpp (Fase 4-6) para reusar sin duplicar entre esa
// plantilla y src/main_zerodisk.cpp (Fase 7) -- ambas necesitan descifrar
// EmbeddedFile y verificar el HMAC-SHA256 de integridad, con el mismo
// formato exacto que genero avapack_gen (ver embedded_project.h).

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "embedded_project.h"

extern "C" {
#include "aes.h" // third_party/tiny-aes-c
}

#include "checksum/sha256.h"

namespace avapack {

// Descifra un EmbeddedFile en memoria (AES-256-CTR, misma operacion para
// cifrar/descifrar -- ver aes.h). `key` ya fue reconstruida una sola vez
// por el llamador. Si avapack::kDebugBuild es true (Fase 5, avapack_gen
// corrio con --debug), f.cipher YA esta en claro -- aplicar
// AES_CTR_xcrypt_buffer igual lo corromperia en vez de "descifrarlo", asi
// que en ese caso esta funcion se reduce a una copia.
inline std::vector<unsigned char> Decrypt(const EmbeddedFile& f, const unsigned char key[32]) {
    std::vector<unsigned char> plaintext(f.content_len);
    if (f.content_len > 0) {
        std::memcpy(plaintext.data(), f.cipher, f.content_len);
        if (!kDebugBuild) {
            struct AES_ctx ctx;
            AES_init_ctx_iv(&ctx, key, f.nonce);
            AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), plaintext.size());
        }
    }
    return plaintext;
}

// Fase 5: recalcula el HMAC-SHA256 sobre TODO kEmbeddedFiles (mismo orden
// y mismo formato de buffer que avapack_gen uso para generar
// kIntegrityMac) y lo compara en tiempo constante contra el valor
// embebido. `key` ya fue reconstruida por el llamador.
inline bool VerifyIntegrity(const unsigned char key[32]) {
    std::vector<unsigned char> mac_input;
    for (std::size_t i = 0; i < kEmbeddedFileCount; ++i) {
        const EmbeddedFile& f = kEmbeddedFiles[i];
        std::size_t path_len = std::strlen(f.path);
        mac_input.insert(mac_input.end(), f.path, f.path + path_len);
        mac_input.insert(mac_input.end(), f.cipher, f.cipher + f.content_len);
        mac_input.insert(mac_input.end(), f.nonce, f.nonce + 16);
    }
    unsigned char computed_mac[32];
    HmacSha256(key, 32, mac_input.empty() ? nullptr : mac_input.data(), mac_input.size(),
               computed_mac);
    std::fill(mac_input.begin(), mac_input.end(), 0);
    return ConstantTimeEquals(computed_mac, kIntegrityMac, 32);
}

// Mapa ruta-relativa-con-'/' -> EmbeddedFile*, construido una vez por
// llamador (kEmbeddedFiles[i].path siempre usa '/', ver embedded_project.h).
using FileMap = std::unordered_map<std::string, const EmbeddedFile*>;

inline FileMap BuildFileMap() {
    FileMap map;
    for (std::size_t i = 0; i < kEmbeddedFileCount; ++i) {
        map[kEmbeddedFiles[i].path] = &kEmbeddedFiles[i];
    }
    return map;
}

} // namespace avapack

#endif // AVAPACK_EMBEDDED_CRYPTO_H
