#ifndef AVAPACK_CHECKSUM_SHA256_H
#define AVAPACK_CHECKSUM_SHA256_H

#include <cstddef>
#include <cstdint>

// Implementacion propia de SHA-256 (FIPS 180-4) + HMAC-SHA256 (RFC 2104),
// escrita desde cero para avapack -- no es una libreria de terceros, por
// eso vive en src/checksum/ y no en third_party/ (ver AVAPACK_STRUCT.md).
// Unico proposito: Fase 5, verificacion de integridad del contenido
// embebido (detectar un binario parcheado, no autenticar nada mas alla de
// eso -- ver runtime/avapack/README.md, seccion Fase 5).
//
// No es de proposito general: no hay streaming incremental mas alla de
// Update()/Final(), y no se optimizo para volumenes grandes (el uso real
// es sobre los archivos .ava/.avaui de un proyecto empacado, tipicamente
// unos pocos KB-MB en total).

namespace avapack {

class Sha256 {
public:
    Sha256();

    void Update(const unsigned char* data, std::size_t len);
    // Escribe el digest de 32 bytes en out_digest y deja el objeto en un
    // estado no reutilizable (llamar Update() despues de Final() es un
    // error de uso -- no hay reset automatico, igual que la mayoria de
    // las implementaciones de referencia de SHA-256).
    void Final(unsigned char out_digest[32]);

private:
    void ProcessBlock(const unsigned char block[64]);

    std::uint32_t state_[8];
    std::uint64_t bit_len_;
    unsigned char buffer_[64];
    std::size_t buffer_len_;
};

// Atajo para el caso comun: hashear un buffer completo de una sola vez.
void Sha256Digest(const unsigned char* data, std::size_t len, unsigned char out_digest[32]);

// HMAC-SHA256 (RFC 2104). `key` puede ser de cualquier longitud (se
// normaliza a 32 bytes internamente via SHA-256 si mide mas de 64 bytes,
// como pide el RFC) -- en avapack se usa siempre con la clave AES-256 ya
// reconstruida (32 bytes), asi que en la practica nunca pasa por esa rama.
void HmacSha256(const unsigned char* key, std::size_t key_len, const unsigned char* data,
                 std::size_t data_len, unsigned char out_mac[32]);

// Comparacion en tiempo constante (no depende del contenido de los datos
// para decidir cuanto tarda) para no filtrar por timing en cuantos bytes
// iniciales coincidian el MAC calculado y el embebido. Mismo espiritu que
// el resto de Fase 5: mitigacion razonable, no una garantia absoluta --
// ver disclaimer de modelo de amenaza en el README.
bool ConstantTimeEquals(const unsigned char* a, const unsigned char* b, std::size_t len);

} // namespace avapack

#endif // AVAPACK_CHECKSUM_SHA256_H
