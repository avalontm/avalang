#include "sha256.h"

#include <cstring>
#include <vector>

namespace avapack {

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline std::uint32_t RotR(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

} // namespace

Sha256::Sha256() : bit_len_(0), buffer_len_(0) {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::ProcessBlock(const unsigned char block[64]) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        std::uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        std::uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
        std::uint32_t S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
        std::uint32_t ch = (e & f) ^ (~e & g);
        std::uint32_t temp1 = h + S1 + ch + kK[i] + w[i];
        std::uint32_t S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
        std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        std::uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::Update(const unsigned char* data, std::size_t len) {
    bit_len_ += static_cast<std::uint64_t>(len) * 8;

    while (len > 0) {
        std::size_t take = 64 - buffer_len_;
        if (take > len) take = len;
        std::memcpy(buffer_ + buffer_len_, data, take);
        buffer_len_ += take;
        data += take;
        len -= take;

        if (buffer_len_ == 64) {
            ProcessBlock(buffer_);
            buffer_len_ = 0;
        }
    }
}

void Sha256::Final(unsigned char out_digest[32]) {
    // Padding estandar de Merlin-Damgard: 0x80, ceros, y la longitud
    // original en bits como big-endian de 64 bits al final del ultimo
    // bloque (o de dos bloques si no entra el padding completo en el
    // que ya estaba a medio llenar).
    unsigned char pad_one = 0x80;
    Update(&pad_one, 1);
    // Update() ya sumo este byte a bit_len_ -- lo compensamos guardando
    // la longitud real ANTES de este ajuste, mas simple: recalculamos
    // aparte para no arrastrar el +8 del byte de padding.
    std::uint64_t original_bit_len = bit_len_ - 8;

    unsigned char zero = 0x00;
    while (buffer_len_ != 56) {
        Update(&zero, 1);
        // Cada llamada a Update() vuelve a sumar bits -- se corrige otra
        // vez abajo con original_bit_len, que es la unica fuente de
        // verdad para el campo de longitud del padding.
    }

    unsigned char len_bytes[8];
    for (int i = 0; i < 8; ++i) {
        len_bytes[i] = static_cast<unsigned char>((original_bit_len >> (56 - i * 8)) & 0xFF);
    }
    // Escritura directa al buffer en vez de via Update(): a esta altura
    // buffer_len_ ya es exactamente 56, agregar estos 8 bytes completa el
    // bloque de 64 sin disparar un ProcessBlock extra de por medio.
    std::memcpy(buffer_ + 56, len_bytes, 8);
    ProcessBlock(buffer_);

    for (int i = 0; i < 8; ++i) {
        out_digest[i * 4] = static_cast<unsigned char>((state_[i] >> 24) & 0xFF);
        out_digest[i * 4 + 1] = static_cast<unsigned char>((state_[i] >> 16) & 0xFF);
        out_digest[i * 4 + 2] = static_cast<unsigned char>((state_[i] >> 8) & 0xFF);
        out_digest[i * 4 + 3] = static_cast<unsigned char>(state_[i] & 0xFF);
    }
}

void Sha256Digest(const unsigned char* data, std::size_t len, unsigned char out_digest[32]) {
    Sha256 hasher;
    hasher.Update(data, len);
    hasher.Final(out_digest);
}

void HmacSha256(const unsigned char* key, std::size_t key_len, const unsigned char* data,
                 std::size_t data_len, unsigned char out_mac[32]) {
    constexpr std::size_t kBlockSize = 64;
    unsigned char key_block[kBlockSize];
    std::memset(key_block, 0, sizeof(key_block));

    if (key_len > kBlockSize) {
        // RFC 2104: si la clave es mas larga que el tamano de bloque, se
        // reemplaza por su propio hash antes de usarla. No aplica en el
        // uso real de avapack (clave AES-256 de 32 bytes), pero se deja
        // implementado para que la funcion sea correcta en general.
        unsigned char hashed_key[32];
        Sha256Digest(key, key_len, hashed_key);
        std::memcpy(key_block, hashed_key, 32);
    } else if (key_len > 0) {
        std::memcpy(key_block, key, key_len);
    }

    unsigned char ipad[kBlockSize];
    unsigned char opad[kBlockSize];
    for (std::size_t i = 0; i < kBlockSize; ++i) {
        ipad[i] = static_cast<unsigned char>(key_block[i] ^ 0x36);
        opad[i] = static_cast<unsigned char>(key_block[i] ^ 0x5c);
    }

    Sha256 inner;
    inner.Update(ipad, kBlockSize);
    inner.Update(data, data_len);
    unsigned char inner_digest[32];
    inner.Final(inner_digest);

    Sha256 outer;
    outer.Update(opad, kBlockSize);
    outer.Update(inner_digest, 32);
    outer.Final(out_mac);

    std::memset(key_block, 0, sizeof(key_block));
    std::memset(ipad, 0, sizeof(ipad));
    std::memset(opad, 0, sizeof(opad));
    std::memset(inner_digest, 0, sizeof(inner_digest));
}

bool ConstantTimeEquals(const unsigned char* a, const unsigned char* b, std::size_t len) {
    unsigned char diff = 0;
    for (std::size_t i = 0; i < len; ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace avapack
