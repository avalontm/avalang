// avapack_gen -- Fase 3 (cifrado) + Fase 5 (integridad / modo debug) +
// Fase 6 (--obfuscate: bytecode precompilado para el entry). Ver
// runtime/avapack/README.md, plan_ava_pack.md y
// runtime/avapack/third_party/tiny-aes-c/VENDOR.md.
//
// Recorre --project buscando archivos .ava/.avaui (decisión de Fase 0: solo
// esas dos extensiones se consideran "código fuente del proyecto" por
// ahora; wwwroot/appsettings.json quedan fuera de este empacador), cifra
// cada uno con AES-256-CTR (clave aleatoria por build, nonce aleatorio por
// archivo) y genera un embedded_project.cpp que define
// avapack::kEmbeddedFiles[] + la clave ofuscada + (Fase 5) el HMAC-SHA256
// de integridad y el flag kDebugBuild, declarados en ../embedded_project.h.
//
// Hasta Fase 5 esto no dependía de avalang -- era una herramienta de host
// standalone que solo tocaba el filesystem, cifraba con tiny-AES-c y
// hasheaba con la implementación propia de src/checksum/. Fase 6 agrega
// --obfuscate, que SÍ linkea avalang -- pero únicamente a través de la API
// pública (avalang.h: ava_compile + ava_module_serialize), nunca headers
// internos del compilador (ver el comentario de cabecera de esa API sobre
// por qué existe). Sin --obfuscate, el flujo es exactamente el de antes y
// no toca avalang para nada.
//
// Uso:
//   avapack_gen --project <dir> --entry <archivo.ava relativo a --project>
//               --out <ruta.cpp> [--key-file <ruta, 32 bytes crudos>]
//               [--debug] [--obfuscate [--obfuscate-strings] [--flatten-control-flow]]
//
// --debug (Fase 5, "modo debug empacado"): no cifra el contenido -- lo deja
// en claro dentro de kEmbeddedFiles[i].cipher, y define kDebugBuild=true
// para que main.cpp sepa que no debe pasarlo por AES_CTR_xcrypt_buffer.
// kIntegrityMac se sigue calculando igual (ver más abajo). Pensado para
// combinarse con un build en modo Debug (símbolos) del propio .exe
// empacado -- eso lo controla `ava_cli build --debug` (Fase 2/5, ver
// runtime/avacli/src/build_command.cpp), no este generador.
//
// --obfuscate (Fase 6): compila el --entry (ava_compile) y lo serializa a
// .avbc (ava_module_serialize) EN VEZ de embeber su .ava en texto plano --
// ese .avbc es lo que se cifra y termina en kEmbeddedFiles para kEntryFile
// (kEntryIsBytecode=true en el .cpp generado). Los imports (todo lo demás)
// siguen siendo .ava/.avaui en texto plano cifrado, sin cambios -- ver
// embedded_project.h para el porqué. --obfuscate-strings/--flatten-control-flow
// son aditivos a --obfuscate (mismos flags que ObfuscateOptions, ver
// compiler/obfuscate.h); si --obfuscate-strings está activo, el .avmap y
// kEntryObfuscateSeed/kEntryStringsObfuscated quedan escritos para que
// main.cpp pueda revertirlo en runtime (ava_module_deobfuscate_strings)
// antes de correr el módulo. El SymbolMap (si algo se ofuscó) se escribe
// junto a --out con extensión .avmap -- NUNCA se embebe en el build.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
#include "aes.h" // third_party/tiny-aes-c, ver VENDOR.md (AES256=1, CTR=1, CBC=0, ECB=0)
}

#include "../checksum/sha256.h" // Fase 5: HMAC-SHA256 propio, ver checksum/sha256.h
#include "avalang.h" // Fase 6: --obfuscate, SOLO API publica (ver comentario de arriba)

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path project_dir;
    std::string entry_file; // relativo a project_dir, separadores '/'
    fs::path out_cpp;
    fs::path key_file; // opcional: 32 bytes crudos con la clave AES-256 a usar
    bool debug_unencrypted = false; // Fase 5: --debug, no cifra el contenido embebido
    std::vector<fs::path> extra_modules_dirs;

    // Fase 6: --obfuscate y sus aditivos.
    bool obfuscate = false;
    bool obfuscate_strings = false;
    bool flatten_control_flow = false;
};

bool ParseArgs(int argc, char** argv, Options& opts, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                error = std::string("falta valor para ") + flag;
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--project") {
            const char* v = next_value("--project");
            if (!v) return false;
            opts.project_dir = v;
        } else if (arg == "--entry") {
            const char* v = next_value("--entry");
            if (!v) return false;
            opts.entry_file = v;
        } else if (arg == "--out") {
            const char* v = next_value("--out");
            if (!v) return false;
            opts.out_cpp = v;
        } else if (arg == "--key-file") {
            const char* v = next_value("--key-file");
            if (!v) return false;
            opts.key_file = v;
        } else if (arg == "--debug") {
            opts.debug_unencrypted = true;
        } else if (arg == "--extra-modules-dir") {
            const char* v = next_value("--extra-modules-dir");
            if (!v) return false;
            opts.extra_modules_dirs.push_back(v);
        } else if (arg == "--obfuscate") {
            opts.obfuscate = true;
        } else if (arg == "--obfuscate-strings") {
            opts.obfuscate_strings = true;
        } else if (arg == "--flatten-control-flow") {
            opts.flatten_control_flow = true;
        } else {
            error = "flag desconocido: " + arg;
            return false;
        }
        if (!error.empty()) return false;
    }
    if (opts.project_dir.empty()) { error = "falta --project"; return false; }
    if (opts.entry_file.empty()) { error = "falta --entry"; return false; }
    if (opts.out_cpp.empty()) { error = "falta --out"; return false; }
    if ((opts.obfuscate_strings || opts.flatten_control_flow) && !opts.obfuscate) {
        error = "--obfuscate-strings/--flatten-control-flow requieren --obfuscate";
        return false;
    }
    return true;
}

std::string ToPosixRelative(const fs::path& base, const fs::path& file) {
    return fs::relative(file, base).generic_string();
}

bool IsEmbeddableExtension(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".ava" || ext == ".avaui";
}

std::vector<std::string> FindImportedModuleNames(const std::string& content) {
    static const std::regex import_re(
        R"(\bimport\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*))");
    std::vector<std::string> out;
    auto begin = std::sregex_iterator(content.begin(), content.end(), import_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string path = (*it)[1].str();
        auto dot = path.find('.');
        out.push_back(dot == std::string::npos ? path : path.substr(0, dot));
    }
    return out;
}

bool ReadFileRaw(const fs::path& p, std::string& out_content) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out_content = ss.str();
    return true;
}

struct FileEntry {
    std::string rel_path;
    std::string content;
};

void EmbedExtraModules(const std::vector<fs::path>& extra_dirs, std::vector<FileEntry>& files) {
    if (extra_dirs.empty()) return;

    std::unordered_set<std::string> resolved;
    std::vector<std::string> pending;
    std::unordered_set<std::string> queued;

    auto queue_name = [&](const std::string& name) {
        if (queued.insert(name).second) pending.push_back(name);
    };

    for (auto& f : files) {
        for (auto& name : FindImportedModuleNames(f.content)) queue_name(name);
    }

    while (!pending.empty()) {
        std::string name = pending.back();
        pending.pop_back();
        if (resolved.count(name)) continue;
        resolved.insert(name);

        bool already_present = std::any_of(files.begin(), files.end(), [&](const FileEntry& f) {
            return f.rel_path == name + ".ava" || f.rel_path.rfind(name + "/", 0) == 0;
        });
        if (already_present) continue;

        for (auto& dir : extra_dirs) {
            fs::path module_dir = dir / name;
            std::error_code ec;
            if (!fs::exists(module_dir, ec) || !fs::is_directory(module_dir, ec)) continue;

            std::vector<FileEntry> module_files;
            for (auto it = fs::recursive_directory_iterator(
                     module_dir, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator() && !ec; it.increment(ec)) {
                const fs::directory_entry& entry = *it;
                std::error_code file_ec;
                if (!entry.is_regular_file(file_ec)) continue;
                if (!IsEmbeddableExtension(entry.path())) continue;

                std::string content;
                if (!ReadFileRaw(entry.path(), content)) continue;
                std::string rel = name + "/" + ToPosixRelative(module_dir, entry.path());
                module_files.push_back({rel, std::move(content)});
            }

            for (auto& mf : module_files) {
                for (auto& imp : FindImportedModuleNames(mf.content)) queue_name(imp);
            }
            for (auto& mf : module_files) files.push_back(std::move(mf));

            std::cout << "avapack_gen: modulo externo '" << name << "' embebido desde "
                      << module_dir.string() << "\n";
            break;
        }
    }
}

// --- utilidades de aleatoriedad para claves/nonces ---
// std::random_device es la mejor fuente de entropía portable disponible sin
// agregar otra dependencia; documentado en el README como parte del modelo
// de amenaza (Fase 3: disuasivo, no un HSM). Si random_device degrada a
// pseudo-aleatorio en alguna plataforma (algunos mingw viejos), sigue
// siendo mejor que una clave fija, que era la alternativa.
void FillRandomBytes(unsigned char* out, std::size_t n) {
    static std::random_device rd;
    static std::mt19937_64 gen(([]() {
        std::random_device seed_rd;
        return (static_cast<std::uint64_t>(seed_rd()) << 32) ^ seed_rd();
    })());
    std::uniform_int_distribution<int> dist(0, 255);
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = static_cast<unsigned char>(dist(gen));
    }
    (void)rd;
}

std::uint32_t RandomU32() {
    unsigned char b[4];
    FillRandomBytes(b, 4);
    return (static_cast<std::uint32_t>(b[0]) << 24) | (static_cast<std::uint32_t>(b[1]) << 16) |
           (static_cast<std::uint32_t>(b[2]) << 8) | static_cast<std::uint32_t>(b[3]);
}

// Debe coincidir bit a bit con avapack::KeyMaskByte en embedded_project.h --
// es la misma función, reimplementada acá porque avapack_gen no linkea
// contra el runtime empacado, para no arrastrar dependencias cruzadas
// (ver AVAPACK_STRUCT.md: "ningún archivo bajo runtime/avapack/ debe ser
// importado por runtime/avalang/", y por simetría este generador tampoco
// depende de esa plantilla). Si se toca una, hay que tocar la otra --
// documentado en ambos lugares.
unsigned char KeyMaskByte(std::uint32_t seed, int index) {
    std::uint32_t x = seed ^ (static_cast<std::uint32_t>(index) * 2654435761u);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return static_cast<unsigned char>(x & 0xFFu);
}

bool ReadKeyFile(const fs::path& p, unsigned char out_key[32], std::string& error) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        error = "no se pudo abrir --key-file: " + p.string();
        return false;
    }
    f.read(reinterpret_cast<char*>(out_key), 32);
    std::streamsize got = f.gcount();
    if (got != 32) {
        error = "--key-file debe contener exactamente 32 bytes crudos (se leyeron " +
                std::to_string(got) + "): " + p.string();
        return false;
    }
    return true;
}

// Emite un array de bytes C++ (`unsigned char name[] = { 0x.., ... };`),
// 16 bytes por línea para que sea legible en un diff, aunque el contenido
// sea binario cifrado. A diferencia de los literales de string de la Fase
// 1, un array de inicializadores no tiene el problema de secuencias
// delimitadoras ni de límites de tamaño de literal -- no hace falta
// trocearlo.
void WriteByteArray(std::ostream& out, const std::string& name, const unsigned char* data,
                     std::size_t len, bool is_static) {
    out << (is_static ? "static " : "") << "const unsigned char " << name << "[] = {";
    char buf[8];
    for (std::size_t i = 0; i < len; ++i) {
        if (i % 16 == 0) out << "\n    ";
        std::snprintf(buf, sizeof(buf), "0x%02x, ", data[i]);
        out << buf;
    }
    if (len == 0) out << "0"; // arrays de tamaño 0 no son válidos en C++ estándar
    out << "\n};\n";
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    std::string err;
    if (!ParseArgs(argc, argv, opts, err)) {
        std::cerr << "avapack_gen: " << err << "\n";
        std::cerr << "uso: avapack_gen --project <dir> --entry <archivo.ava> --out <ruta.cpp> "
                     "[--key-file <ruta>] [--debug] [--extra-modules-dir <dir>]... "
                     "[--obfuscate [--obfuscate-strings] [--flatten-control-flow]]\n";
        return 1;
    }

    std::error_code ec;
    if (!fs::exists(opts.project_dir, ec) || !fs::is_directory(opts.project_dir, ec)) {
        std::cerr << "avapack_gen: --project no existe o no es un directorio: "
                  << opts.project_dir.string() << "\n";
        return 1;
    }

    fs::path entry_abs = opts.project_dir / fs::path(opts.entry_file);
    if (!fs::exists(entry_abs, ec)) {
        std::cerr << "avapack_gen: --entry no existe dentro de --project: "
                  << opts.entry_file << "\n";
        return 1;
    }
    if (!IsEmbeddableExtension(entry_abs)) {
        std::cerr << "avapack_gen: --entry debe ser .ava (o .avaui): " << opts.entry_file << "\n";
        return 1;
    }

    std::vector<FileEntry> files;

    for (auto it = fs::recursive_directory_iterator(
             opts.project_dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            std::cerr << "avapack_gen: error recorriendo el proyecto: " << ec.message() << "\n";
            return 1;
        }
        const fs::directory_entry& entry = *it;
        std::error_code file_ec;
        if (!entry.is_regular_file(file_ec)) continue;
        if (!IsEmbeddableExtension(entry.path())) continue;

        std::string content;
        if (!ReadFileRaw(entry.path(), content)) {
            std::cerr << "avapack_gen: no se pudo leer " << entry.path().string() << "\n";
            return 1;
        }
        files.push_back({ToPosixRelative(opts.project_dir, entry.path()), std::move(content)});
    }

    if (files.empty()) {
        std::cerr << "avapack_gen: no se encontro ningun .ava/.avaui dentro de --project\n";
        return 1;
    }

    EmbedExtraModules(opts.extra_modules_dirs, files);

    // Orden determinista: la misma entrada + la misma clave producen siempre el mismo .cpp
    // (el cifrado en sí no es determinista porque el nonce es aleatorio por archivo, pero el
    // orden de la lista sí, para que los diffs del .cpp generado sean legibles si algún día se
    // decide commitear uno de referencia).
    std::sort(files.begin(), files.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.rel_path < b.rel_path; });

    std::string entry_rel = ToPosixRelative(opts.project_dir, entry_abs);
    bool entry_in_list = std::any_of(files.begin(), files.end(), [&](const FileEntry& f) {
        return f.rel_path == entry_rel;
    });
    if (!entry_in_list) {
        // No debería pasar (ya validamos extensión arriba), pero por las dudas.
        std::cerr << "avapack_gen: --entry (" << entry_rel << ") no quedo en la lista embebida\n";
        return 1;
    }

    // --- Fase 6: --obfuscate reemplaza el contenido del entry (texto .ava)
    //     por su forma compilada+serializada (.avbc) ANTES de que el loop
    //     de cifrado de mas abajo toque nada -- para el resto del pipeline
    //     (AES-256-CTR, HMAC de integridad) el entry es solo otro blob de
    //     bytes, no le importa si es texto o bytecode. ---
    bool entry_is_bytecode = false;
    bool entry_strings_obfuscated = false;
    std::uint64_t entry_obfuscate_seed = 0;
    if (opts.obfuscate) {
        auto entry_it = std::find_if(files.begin(), files.end(),
            [&](const FileEntry& f) { return f.rel_path == entry_rel; });
        if (entry_it == files.end()) {
            // No debería pasar (entry_in_list, validado más arriba, ya lo
            // confirma), pero por las dudas no seguimos con un iterador inválido.
            std::cerr << "avapack_gen: --obfuscate: entry no encontrado en la lista de archivos\n";
            return 1;
        }

        entry_obfuscate_seed = (static_cast<std::uint64_t>(RandomU32()) << 32) | RandomU32();

        AvaVM* vm = ava_vm_create();
        char* compile_error = nullptr;
        AvaModule* module = ava_compile(vm, entry_it->content.c_str(), entry_rel.c_str(), &compile_error);
        if (!module) {
            std::cerr << "avapack_gen: --obfuscate: el entry no compiló: "
                      << (compile_error ? compile_error : "unknown error") << "\n";
            if (compile_error) ava_string_free(compile_error);
            ava_vm_destroy(vm);
            return 1;
        }

        AvaModuleSerializeOptions sopts{};
        sopts.strip_debug_info = 1;
        sopts.obfuscate = 1;
        sopts.obfuscate_seed = entry_obfuscate_seed;
        sopts.obfuscate_strings = opts.obfuscate_strings ? 1 : 0;
        sopts.flatten_control_flow = opts.flatten_control_flow ? 1 : 0;

        size_t bc_len = 0;
        char* symbol_map = nullptr;
        uint8_t* bc = ava_module_serialize(module, &sopts, &bc_len, &symbol_map);
        // Orden invertido: ver comentario en ava_barekernel_runner.cpp
        // sobre el use-after-free de teardown.
        ava_vm_destroy(vm);
        ava_module_destroy(module);
        if (!bc) {
            std::cerr << "avapack_gen: --obfuscate: ava_module_serialize devolvió NULL\n";
            if (symbol_map) ava_string_free(symbol_map);
            return 1;
        }

        entry_it->content.assign(reinterpret_cast<char*>(bc), bc_len);
        ava_string_free(reinterpret_cast<char*>(bc));
        entry_is_bytecode = true;
        entry_strings_obfuscated = opts.obfuscate_strings;

        if (symbol_map && symbol_map[0] != '\0') {
            fs::path avmap_path = opts.out_cpp;
            avmap_path.replace_extension(".avmap");
            std::ofstream avmap_out(avmap_path, std::ios::binary);
            if (avmap_out) {
                avmap_out << symbol_map;
                std::cout << "avapack_gen: --obfuscate: SymbolMap escrito en "
                          << avmap_path.string() << " -- NO distribuir junto al .exe\n";
            } else {
                std::cerr << "avapack_gen: aviso: no se pudo escribir " << avmap_path.string() << "\n";
            }
        }
        if (symbol_map) ava_string_free(symbol_map);

        std::cout << "avapack_gen: --obfuscate: entry (" << entry_rel << ") compilado y serializado a "
                   << "bytecode (" << bc_len << " bytes)"
                   << (opts.obfuscate_strings ? ", strings ofuscados" : "")
                   << (opts.flatten_control_flow ? ", control-flow aplanado" : "") << "\n";
    }

    // --- Fase 3: clave AES-256 (aleatoria por build, o la de --key-file) ---
    unsigned char real_key[32];
    if (!opts.key_file.empty()) {
        std::string key_err;
        if (!ReadKeyFile(opts.key_file, real_key, key_err)) {
            std::cerr << "avapack_gen: " << key_err << "\n";
            return 1;
        }
    } else {
        FillRandomBytes(real_key, 32);
    }
    std::uint32_t key_seed = RandomU32();
    unsigned char key_fragment_a[16];
    unsigned char key_fragment_b[16];
    for (int i = 0; i < 16; ++i) key_fragment_a[i] = real_key[i] ^ KeyMaskByte(key_seed, i);
    for (int i = 0; i < 16; ++i) key_fragment_b[i] = real_key[16 + i] ^ KeyMaskByte(key_seed, 16 + i);

    // --- cifrar cada archivo con AES-256-CTR, nonce aleatorio de 16 bytes propio ---
    struct EncryptedEntry {
        std::string rel_path;
        std::vector<unsigned char> cipher;
        unsigned char nonce[16];
    };
    std::vector<EncryptedEntry> encrypted;
    encrypted.reserve(files.size());

    for (auto& f : files) {
        EncryptedEntry e;
        e.rel_path = f.rel_path;
        FillRandomBytes(e.nonce, 16);

        e.cipher.assign(f.content.begin(), f.content.end());

        // --debug (Fase 5): el nonce se genera igual (mantiene el formato
        // de kEmbeddedFiles estable entre builds normales y de debug) pero
        // no se usa -- el contenido queda en claro para poder diagnosticar
        // el proyecto empacado sin descifrar nada a mano. Ver
        // embedded_project.h, comentario de kDebugBuild.
        if (!opts.debug_unencrypted) {
            struct AES_ctx ctx;
            AES_init_ctx_iv(&ctx, real_key, e.nonce);
            if (!e.cipher.empty()) {
                AES_CTR_xcrypt_buffer(&ctx, e.cipher.data(),
                                       static_cast<std::size_t>(e.cipher.size()));
            }
        }
        encrypted.push_back(std::move(e));
    }

    // --- Fase 5: HMAC-SHA256 de integridad sobre TODO el contenido embebido ---
    // Concatena path + cipher + nonce de cada entrada, en el mismo orden
    // determinista de `encrypted` (ya viene de `files`, ordenado por
    // rel_path más arriba) -- main.cpp reconstruye exactamente el mismo
    // buffer a partir de avapack::kEmbeddedFiles para poder recalcular el
    // mismo HMAC y compararlo. Se firma con la clave AES-256 real (antes de
    // partirla en fragmentos ofuscados) -- no se genera un secreto nuevo
    // para esto, ver comentario en embedded_project.h.
    std::vector<unsigned char> mac_input;
    for (const auto& e : encrypted) {
        mac_input.insert(mac_input.end(), e.rel_path.begin(), e.rel_path.end());
        mac_input.insert(mac_input.end(), e.cipher.begin(), e.cipher.end());
        mac_input.insert(mac_input.end(), e.nonce, e.nonce + 16);
    }
    unsigned char integrity_mac[32];
    avapack::HmacSha256(real_key, 32, mac_input.empty() ? nullptr : mac_input.data(),
                         mac_input.size(), integrity_mac);
    std::fill(mac_input.begin(), mac_input.end(), 0); // no dejar el buffer de firma en claro

    fs::create_directories(opts.out_cpp.parent_path(), ec);
    std::ofstream out(opts.out_cpp, std::ios::binary);
    if (!out) {
        std::cerr << "avapack_gen: no se pudo escribir " << opts.out_cpp.string() << "\n";
        return 1;
    }

    out << "// Generado por avapack_gen -- NO EDITAR A MANO.\n";
    out << "// Fuente: " << opts.project_dir.string() << " (entry: " << entry_rel << ")\n";
    out << "// Contenido cifrado con AES-256-CTR; ver embedded_project.h y\n";
    out << "// runtime/avapack/third_party/tiny-aes-c/VENDOR.md.\n";
    out << "#include \"embedded_project.h\"\n\n";
    out << "namespace avapack {\n\n";
    out << "namespace {\n";

    for (size_t i = 0; i < encrypted.size(); ++i) {
        WriteByteArray(out, "kFileCipher" + std::to_string(i), encrypted[i].cipher.data(),
                        encrypted[i].cipher.size(), /*is_static=*/true);
        WriteByteArray(out, "kFileNonce" + std::to_string(i), encrypted[i].nonce, 16,
                        /*is_static=*/true);
        out << "\n";
    }

    out << "} // namespace\n\n";

    out << "const EmbeddedFile kEmbeddedFiles[] = {\n";
    for (size_t i = 0; i < encrypted.size(); ++i) {
        out << "    { \"" << encrypted[i].rel_path << "\", kFileCipher" << i << ", "
            << encrypted[i].cipher.size() << "u, kFileNonce" << i << " },\n";
    }
    out << "};\n\n";
    out << "const std::size_t kEmbeddedFileCount = sizeof(kEmbeddedFiles) / sizeof(kEmbeddedFiles[0]);\n";
    out << "const char* const kEntryFile = \"" << entry_rel << "\";\n\n";

    out << "const std::uint32_t kKeySeed = " << key_seed << "u;\n";
    WriteByteArray(out, "kKeyFragmentA", key_fragment_a, 16, /*is_static=*/false);
    WriteByteArray(out, "kKeyFragmentB", key_fragment_b, 16, /*is_static=*/false);

    out << "\n";
    WriteByteArray(out, "kIntegrityMac", integrity_mac, 32, /*is_static=*/false);
    out << "const bool kDebugBuild = " << (opts.debug_unencrypted ? "true" : "false") << ";\n";

    out << "\n// Fase 6:\n";
    out << "const bool kEntryIsBytecode = " << (entry_is_bytecode ? "true" : "false") << ";\n";
    out << "const bool kEntryStringsObfuscated = " << (entry_strings_obfuscated ? "true" : "false") << ";\n";
    out << "const std::uint64_t kEntryObfuscateSeed = " << entry_obfuscate_seed << "ull;\n";

    out << "\n} // namespace avapack\n";

    std::cout << "avapack_gen: " << files.size() << " archivo(s) embebido(s) y cifrado(s), entry="
              << entry_rel << " -> " << opts.out_cpp.string() << "\n";
    if (opts.key_file.empty()) {
        std::cout << "avapack_gen: clave AES-256 generada al azar para este build "
                     "(usa --key-file para fijar una).\n";
    } else {
        std::cout << "avapack_gen: clave AES-256 leida de " << opts.key_file.string() << "\n";
    }
    if (opts.debug_unencrypted) {
        std::cout << "avapack_gen: --debug activo -- el contenido embebido QUEDA EN CLARO, "
                     "no usar para distribuir a produccion.\n";
    }
    std::memset(real_key, 0, sizeof(real_key)); // ya no hace falta -- ni siquiera para el HMAC
    return 0;
}
