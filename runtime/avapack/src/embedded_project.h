#ifndef AVAPACK_EMBEDDED_PROJECT_H
#define AVAPACK_EMBEDDED_PROJECT_H

#include <cstddef>
#include <cstdint>

// Contrato compartido entre avapack_gen (src/generator/, produce un
// embedded_project.cpp que define estos símbolos) y la plantilla de runtime
// empacado (src/main.cpp, los consume). Ver AVAPACK_STRUCT.md.
//
// Fase 3: EmbeddedFile::cipher ya NO es texto plano -- es el resultado de
// cifrar el archivo original con AES-256-CTR (ver
// runtime/avapack/third_party/tiny-aes-c/VENDOR.md sobre por qué CTR y no
// GCM), con un nonce/IV de 16 bytes distinto por archivo. `strings` sobre
// el .exe final ya no debe mostrar código AvaLang en claro -- ver
// runtime/avapack/README.md, sección Fase 3, para el detalle y el modelo
// de amenaza (esto es disuasivo, no resistente a un atacante con
// debugger/volcado de memoria en tiempo de ejecución).
//
// Fase 5 agrega dos campos nuevos al contrato, declarados más abajo:
// kIntegrityMac (HMAC-SHA256 del contenido embebido, anti-tampering
// básico) y kDebugBuild (si --debug generó el .cpp con contenido en claro
// en vez de cifrado). Ninguno de los dos cambia el struct EmbeddedFile en
// sí -- son símbolos nuevos a nivel de namespace, no rompen builds
// generados en fases anteriores... salvo que sí lo hacen en el sentido de
// que un embedded_project.cpp viejo (Fase 3/4) no define kIntegrityMac ni
// kDebugBuild y no linkeará contra este header nuevo. No hay compatibilidad
// binaria entre .cpp generados antes/después de Fase 5 -- hay que
// regenerar con el avapack_gen nuevo (documentado también en el README).

namespace avapack {

struct EmbeddedFile {
    const char* path;              // ruta relativa al proyecto, siempre con '/' como separador
                                    // (sin cifrar a propósito: no es el contenido protegido).
    const unsigned char* cipher;   // ciphertext AES-256-CTR; longitud == content_len (CTR no
                                    // agrega padding, el tamaño del cifrado es igual al del
                                    // plaintext original).
    std::size_t content_len;       // longitud en bytes del archivo original (== longitud de cipher).
    const unsigned char* nonce;    // IV/contador inicial de 16 bytes (AES_BLOCKLEN), único por
                                    // archivo dentro del build -- nunca se reutiliza con la
                                    // misma clave (requisito de seguridad de CTR).
};

extern const EmbeddedFile kEmbeddedFiles[];
extern const std::size_t kEmbeddedFileCount;
extern const char* const kEntryFile; // ruta relativa (dentro de kEmbeddedFiles) del entry point

// --- Fase 6: entry como bytecode precompilado (.avbc) en vez de fuente ---
//
// Cuando avapack_gen corre con --obfuscate, el EmbeddedFile cuyo path ==
// kEntryFile NO contiene el .ava original -- contiene el resultado de
// compilarlo (avalang.h::ava_compile) y serializarlo (::ava_module_serialize,
// ver runtime/avalang/src/compiler/proto_io.h) a formato .avbc, cifrado con
// el mismo esquema AES-256-CTR de siempre. kEntryIsBytecode le indica a
// main.cpp que, tras descifrar, debe llamar ava_module_deserialize en vez
// de ava_compile -- el resto del flujo (VerifyIntegrity, hooks de imports,
// etc.) no cambia.
//
// Los imports (todo lo demas en kEmbeddedFiles) siguen siendo .ava/.avaui
// en texto plano cifrado, sin cambios -- precompilar el arbol completo de
// imports requiere que ModuleResolver/DoImport (vm_import.cpp) sepan
// deserializar .avbc en vez de solo compilar texto, que queda fuera de
// esta fase (ver "Nota de dependencia" en plan_ava_pack.md Fase 6).
//
// Sin --obfuscate, kEntryIsBytecode es false y el flujo es exactamente el
// de Fases 1-5 (kEntryFile es .ava en texto plano cifrado, igual que
// cualquier import).
extern const bool kEntryIsBytecode;

// Si kEntryIsBytecode y el build ademas pidio ofuscar strings
// (ObfuscateOptions::obfuscate_strings), la constant pool del entry queda
// con sus Value::String cifrados -- main.cpp debe llamar
// ava_module_deobfuscate_strings(module, kEntryObfuscateSeed) inmediatamente
// despues de deserializar y ANTES de ava_run. kEntryStringsObfuscated en
// false significa que no hace falta (incluye el caso kEntryIsBytecode ==
// false, donde ninguno de estos tres simbolos es relevante).
extern const bool kEntryStringsObfuscated;
extern const std::uint64_t kEntryObfuscateSeed;

// --- Fase 5: verificacion de integridad (anti-tampering basico) ---
//
// HMAC-SHA256 (ver src/checksum/sha256.h, implementacion propia) calculado por avapack_gen
// sobre la concatenacion de path+cipher+nonce de cada archivo embebido, en el mismo orden
// determinista en que aparecen en kEmbeddedFiles, usando como clave la MISMA clave AES-256 ya
// reconstruida (avapack::GetEmbeddedKey) -- no se agrega un secreto nuevo al binario, se
// reutiliza el que ya existia. La plantilla de runtime (main.cpp) recalcula este HMAC al
// arrancar, antes de descifrar nada, y aborta si no coincide con kIntegrityMac.
//
// Esto detecta: alguien parcheo bytes del array kEmbeddedFiles/kIntegrityMac dentro del .exe
// ya compilado (ej. para intentar reemplazar un archivo cifrado por otro, o para invalidar el
// chequeo a mano sin recompilar). NO previene tampering: un atacante dispuesto a parchear el
// binario tambien puede parchear el propio chequeo (saltar el branch, o recalcular el HMAC con
// la clave que ya tiene que poder reconstruir para que el programa funcione). Mismo disclaimer
// que el resto de Fase 3/4: disuasivo contra manipulacion casual, no una garantia criptografica
// de que el binario no fue tocado.
extern const unsigned char kIntegrityMac[32];

// --- Fase 5: modo debug empacado ---
//
// Cuando avapack_gen corre con --debug, kEmbeddedFiles[i].cipher contiene el contenido
// ORIGINAL en claro (no cifrado) de cada archivo -- pensado para poder diagnosticar bugs de un
// proyecto ya empacado sin tener que descifrar nada a mano, a costa de perder la proteccion de
// Fase 3 (ver runtime/avapack/README.md, seccion Fase 5). kDebugBuild le indica a main.cpp que
// NO debe pasar ese contenido por AES_CTR_xcrypt_buffer (aplicar CTR sobre texto que ya esta en
// claro lo corromperia, no lo "descifraria" -- CTR es su propia inversa solo cuando el otro
// lado realmente cifro con la misma clave/nonce). kIntegrityMac se sigue calculando y
// verificando igual en modo debug (sobre el contenido en claro) -- detectar tampering no
// depende de que el contenido este cifrado.
extern const bool kDebugBuild;

// --- Clave AES-256 embebida, ofuscada ---
//
// La clave real (32 bytes) nunca existe como bloque contiguo en el binario. Se genera al
// azar por build (avapack_gen), se parte en dos mitades de 16 bytes, y cada mitad se guarda
// XOReada contra una máscara calculada en tiempo de compilación a partir de kKeySeed (un
// entero de 32 bits también aleatorio por build). Kerckhoffs de por medio: el algoritmo de
// ofuscación es público (está acá, en el header); lo que cambia por build son kKeySeed y los
// dos fragmentos, así que conocer este código no le da a nadie la clave de OTRO build.
//
// Importante -- esto es disuasivo, no seguridad real: la clave TIENE que poder reconstruirse
// dentro del propio binario para que el programa funcione, así que un atacante dispuesto a
// poner un breakpoint en GetEmbeddedKey() o volcar la memoria del proceso la obtiene igual.
// Lo que esto evita es que la clave aparezca como 32 bytes reconocibles ante un `strings` o
// un escaneo estático simple. Ver runtime/avapack/README.md, sección Fase 3.

constexpr unsigned char KeyMaskByte(std::uint32_t seed, int index) {
    std::uint32_t x = seed ^ (static_cast<std::uint32_t>(index) * 2654435761u);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return static_cast<unsigned char>(x & 0xFFu);
}

extern const std::uint32_t kKeySeed;
extern const unsigned char kKeyFragmentA[16]; // bytes 0..15 de la clave real, XOReados
extern const unsigned char kKeyFragmentB[16]; // bytes 16..31 de la clave real, XOReados

// Fase 9: misma reconstruccion de arriba, pero parametrizada -- GetEmbeddedKey
// (compile-time, via los symbols kKeySeed/kKeyFragmentA/B de un
// embedded_project.cpp generado) sigue igual y ahora delega aca. El stub
// precompilado (runtime/avapack/src/stub_main.cpp) no tiene esos symbols
// compilados adentro -- los lee del payload apendeado en runtime, asi que
// necesita la misma formula pero recibiendo seed/fragmentos como
// parametros en vez de como constantes del binario. Ver payload_format.h.
inline void GetKeyFromFragments(std::uint32_t seed, const unsigned char frag_a[16],
                                 const unsigned char frag_b[16], unsigned char out_key[32]) {
    for (int i = 0; i < 16; ++i) {
        out_key[i] = static_cast<unsigned char>(frag_a[i] ^ KeyMaskByte(seed, i));
    }
    for (int i = 0; i < 16; ++i) {
        out_key[16 + i] = static_cast<unsigned char>(frag_b[i] ^ KeyMaskByte(seed, 16 + i));
    }
}

inline void GetEmbeddedKey(unsigned char out_key[32]) {
    GetKeyFromFragments(kKeySeed, kKeyFragmentA, kKeyFragmentB, out_key);
}

} // namespace avapack

#endif // AVAPACK_EMBEDDED_PROJECT_H
