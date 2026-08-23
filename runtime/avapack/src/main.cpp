// Plantilla de runtime empacado -- Fase 4 (minimizar ventana de exposicion
// en disco. Ver runtime/avapack/README.md y plan_ava_pack.md).
// Este archivo se compila junto con el embedded_project.cpp que genera
// avapack_gen para producir el .exe final.
//
// Flujo (cambia respecto a Fase 3):
//   1) Crea un directorio temporal unico, VACIO (ya no se vuelca todo el
//      proyecto ahi de entrada).
//   2) Reconstruye la clave AES-256 una vez (avapack::GetEmbeddedKey).
//   3) Descifra el entry file EN MEMORIA (nunca toca disco). Si el build se
//      generó con --obfuscate (Fase 6, ver avapack::kEntryIsBytecode), esos
//      bytes son un módulo .avbc precompilado y se deserializan directo
//      (ava_module_deserialize) -- si no, son el .ava en texto plano de
//      siempre y se compilan (ava_compile(vm, source, kEntryFile, ...)).
//   4) Instala dos hooks en la VM (ava::VM::SetBeforeModuleReadHook /
//      SetAfterModuleReadHook, ver runtime/avalang/src/vm/vm.h) que se
//      disparan justo antes/despues de que DoImport (vm_import.cpp) abre
//      el archivo de un modulo importado:
//        - Before: descifra ESE archivo puntual y lo escribe en la ruta
//          que DoImport esta por abrir (y en Windows lo marca
//          FILE_ATTRIBUTE_TEMPORARY).
//        - After: sobreescribe esos bytes en disco con ceros y borra el
//          archivo -- ya se leyo a memoria, DoImport no lo vuelve a
//          tocar (ModuleCache cachea el Proto compilado, ver
//          runtime/avalang/src/vm/module.h/.cpp).
//      Resultado: como mucho un archivo .ava en claro existe en disco a
//      la vez, y por el tiempo que tarda un std::ifstream en leerlo --
//      no durante toda la ejecucion del programa como en Fase 3.
//   5) Borra el directorio temporal al salir, en cualquier camino (RAII,
//      TempDirGuard) -- red de seguridad por si algun archivo no se pudo
//      borrar individualmente (p.ej. el programa crashea a mitad de un
//      import), no el mecanismo principal de limpieza.
//
// Limitacion conocida (documentada tambien en README.md): esto reduce la
// ventana de exposicion, no la elimina -- Fase 7 (filesystem virtual en
// memoria) es el unico disenio que evita tocar disco por completo. Ver
// plan_ava_pack.md.
//
// Fase 5 agrega, ANTES que nada de lo anterior: verificacion de integridad
// (HMAC-SHA256 de todo kEmbeddedFiles, ver embedded_project.h) apenas se
// reconstruye la clave -- si no coincide con kIntegrityMac, el programa
// aborta sin descifrar ni compilar nada. Tambien respeta kDebugBuild: si
// avapack_gen corrio con --debug, el contenido embebido esta en claro y
// Decrypt() no debe aplicarle AES_CTR_xcrypt_buffer encima.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "avalang.h"
#include "vm/vm.h"
#include "embedded_project.h"
#include "embedded_crypto.h" // Decrypt/VerifyIntegrity/FileMap, compartido con main_zerodisk.cpp (Fase 7)

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

// Crea un directorio temporal con nombre unico bajo el temp del sistema.
// En Windows esto ya es %TEMP% del usuario actual (no un temp compartido
// del sistema) -- lo que pide el punto de Fase 4 sobre "carpeta temp
// restringida al usuario actual" ya se cumple sin codigo adicional ahi.
// Queda vacio hasta que los hooks de abajo materializan archivos uno a
// uno (a diferencia de Fase 3, que volcaba todo el proyecto aca mismo).
fs::path MakeTempDir() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    fs::path base = fs::temp_directory_path();
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::ostringstream name;
        name << "avapack_" << std::hex << dist(gen);
        fs::path candidate = base / name.str();
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {}; // no se pudo crear tras varios intentos -- el llamador lo trata como error
}

// RAII: borra el directorio temporal (y cualquier resto que haya quedado
// dentro) al salir de main(). Con los hooks de Fase 4 puestos, en el
// camino feliz esto ya deberia encontrar el directorio vacio -- es una
// red de seguridad para caminos de error/crash, no el mecanismo
// principal de limpieza (ver comentario de cabecera).
struct TempDirGuard {
    fs::path dir;
    ~TempDirGuard() {
        if (!dir.empty()) {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    }
};

#ifdef _WIN32
std::wstring Widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

// Marca el archivo como temporal para el cache del SO: Windows evita
// bajarlo a disco desde el file cache mientras pueda mantenerlo en
// memoria, y algunos limpiadores/backups lo tratan distinto. Es una
// mitigacion adicional, no una garantia -- si el SO igual decide
// escribirlo a disco (memoria bajo presion), el contenido en claro
// termina ahi de todas formas por la ventana que dure abierto.
void MarkTemporary(const fs::path& path) {
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_TEMPORARY);
}
#else
void MarkTemporary(const fs::path&) {
    // No implementado fuera de Windows -- ver Fase 8 (multiplataforma),
    // hoy bloqueada por platform/linux|macos siendo stub. No es un
    // no-op silencioso grave: en esas plataformas avapack tampoco
    // compila/corre todavia por otras razones (avalang.dll PAL).
}
#endif

// Sobreescribe el contenido del archivo con ceros antes de borrarlo --
// mitigacion de "no dejar el plaintext recuperable con un undelete
// trivial" para el archivo que YA se leyo a memoria. No es un secure
// erase a nivel de sector (SSDs con wear-leveling, journaling
// filesystems, etc. pueden conservar copias fuera de nuestro control --
// mismo disclaimer de "disuasivo, no criptograficamente inexpugnable"
// que ya tiene el resto de Fase 3, ver embedded_project.h).
void ZeroAndRemove(const fs::path& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (!ec && size > 0) {
        std::ofstream out(path, std::ios::binary | std::ios::in | std::ios::out);
        if (out) {
            std::vector<char> zeros(static_cast<std::size_t>(size), 0);
            out.write(zeros.data(), zeros.size());
        }
    }
    fs::remove(path, ec);
}

// Decrypt/VerifyIntegrity/FileMap (avapack::) ahora viven en
// embedded_crypto.h, compartidos con main_zerodisk.cpp (Fase 7).
using avapack::Decrypt;
using avapack::VerifyIntegrity;
using avapack::FileMap;
using avapack::BuildFileMap;

std::string ToRelativePosix(const fs::path& temp_dir, const std::string& resolved_path) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(resolved_path), temp_dir, ec);
    if (ec) return "";
    return rel.generic_string(); // generic_string() ya usa '/' sin importar el SO
}

// Expone los argumentos de linea de comandos del .exe empacado como el
// global `args` (una List de strings) para que el entry .ava los pueda
// leer -- p.ej. `miapp.exe par1 par2` adentro del script queda como
// args = ["par1", "par2"]. argv[0] (el path del propio .exe) queda
// deliberadamente afuera, igual que sys.argv[1:] en Python.
void SetScriptArgsGlobal(ava::VM* raw_vm, int argc, char** argv) {
    auto* list = new ava::ListObj();
    for (int i = 1; i < argc; ++i) {
        list->items.push_back(ava::Value::String(argv[i]));
    }
    ava::Value args_value;
    args_value.type = ava::ValueType::List;
    args_value.obj = list;
    raw_vm->SetGlobal("args", args_value);
}

} // namespace

int main(int argc, char** argv) {
    fs::path temp_dir = MakeTempDir();
    if (temp_dir.empty()) {
        std::fprintf(stderr, "error: no se pudo crear directorio temporal\n");
        return 1;
    }
    TempDirGuard temp_guard{temp_dir};

    unsigned char key[32];
    avapack::GetEmbeddedKey(key);

    // Fase 5: antes de descifrar o compilar cualquier cosa, confirmar que
    // el contenido embebido no fue parcheado despues de generarse. Esto es
    // deteccion, no prevencion (ver embedded_project.h) -- pero un binario
    // que falla este chequeo no llega a exponer ni un byte de codigo
    // descifrado.
    if (!VerifyIntegrity(key)) {
        std::fprintf(stderr,
                      "error: verificacion de integridad fallida -- el contenido embebido "
                      "no coincide con el esperado (binario posiblemente modificado)\n");
        std::memset(key, 0, sizeof(key));
        return 1;
    }

    FileMap file_map = BuildFileMap();

    // El entry file se descifra directo a memoria y nunca toca disco en
    // claro -- a diferencia de los imports (que DoImport abre por ruta de
    // disco via ModuleResolver, ver vm_import.cpp). Segun kEntryIsBytecode
    // (Fase 6), esos bytes en claro son o bien fuente .ava (se compilan,
    // flujo de siempre) o bien un modulo .avbc precompilado (se
    // deserializan directo, sin pasar por el frontend ANTLR en absoluto).
    auto entry_it = file_map.find(avapack::kEntryFile);
    if (entry_it == file_map.end()) {
        std::fprintf(stderr, "error: entry file no encontrado entre los archivos embebidos: %s\n",
                     avapack::kEntryFile);
        std::memset(key, 0, sizeof(key));
        return 1;
    }
    std::vector<unsigned char> entry_plain = Decrypt(*entry_it->second, key);

    AvaVM* vm = ava_vm_create();
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    raw_vm->GetModuleResolver().AddSearchPath(temp_dir.string());
    SetScriptArgsGlobal(raw_vm, argc, argv);

    // Hooks de Fase 4 (ver runtime/avalang/src/vm/vm.h): se disparan
    // desde DoImport, dentro de la propia avalang.dll, justo
    // antes/despues de que abra el archivo resuelto de un import.
    raw_vm->SetBeforeModuleReadHook([&file_map, &key, &temp_dir](const std::string& resolved_path) {
        std::string rel = ToRelativePosix(temp_dir, resolved_path);
        auto it = file_map.find(rel);
        if (it == file_map.end()) {
            // No es uno de nuestros archivos embebidos (no deberia pasar
            // -- ModuleResolver solo encuentra archivos que existen bajo
            // temp_dir, y lo unico que existe ahi es lo que nosotros
            // mismos vamos materializando). Se deja pasar sin escribir
            // nada; DoImport fallara con su propio error de "no se pudo
            // abrir", que es preferible a un descifrado incorrecto.
            return;
        }
        std::vector<unsigned char> plaintext = Decrypt(*it->second, key);

        fs::path out_path(resolved_path);
        std::error_code ec;
        fs::create_directories(out_path.parent_path(), ec);

        std::ofstream out(out_path, std::ios::binary);
        if (out && !plaintext.empty()) {
            out.write(reinterpret_cast<const char*>(plaintext.data()),
                       static_cast<std::streamsize>(plaintext.size()));
        }
        out.close();
        if (!plaintext.empty()) {
            std::memset(plaintext.data(), 0, plaintext.size());
        }
        MarkTemporary(out_path);
    });

    raw_vm->SetAfterModuleReadHook([](const std::string& resolved_path) {
        ZeroAndRemove(fs::path(resolved_path));
    });

    char* error = nullptr;
    AvaModule* module = nullptr;
    if (avapack::kEntryIsBytecode) {
        // Fase 6: el entry ya es un Proto serializado (.avbc, ver
        // runtime/avalang/src/compiler/proto_io.h) -- se reconstruye
        // directo, sin pasar por ava_compile/el frontend ANTLR. Si el
        // build ademas ofusco strings (avapack::kEntryStringsObfuscated),
        // hay que revertirlo ANTES de ava_run -- ver comentario de
        // ava_module_deobfuscate_strings en avalang.h sobre por que no
        // puede diferirse.
        module = ava_module_deserialize(vm, entry_plain.data(), entry_plain.size(), &error);
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        if (module && avapack::kEntryStringsObfuscated) {
            ava_module_deobfuscate_strings(module, avapack::kEntryObfuscateSeed);
        }
        if (!module) {
            std::fprintf(stderr, "error: entry .avbc invalido: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, sizeof(key));
            ava_vm_destroy(vm);
            return 1;
        }
    } else {
        std::string entry_source(entry_plain.begin(), entry_plain.end());
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        module = ava_compile(vm, entry_source.c_str(), avapack::kEntryFile, &error);
        entry_source.assign(entry_source.size(), '\0'); // higiene: no dejar la fuente en claro mas de lo necesario
        if (!module) {
            std::fprintf(stderr, "compile error: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, sizeof(key));
            ava_vm_destroy(vm);
            return 1;
        }
    }

    ava_value_t result{};
    ava_run(vm, module, &result, &error);
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        std::memset(key, 0, sizeof(key));
        // Orden invertido: ver comentario en ava_barekernel_runner.cpp
        // sobre el use-after-free de teardown.
        ava_vm_destroy(vm);
        ava_module_destroy(module);
        return 1;
    }

    // Igual que ava_cli: drenar timers/callbacks async pendientes antes de
    // salir (ver Fase 5 del Async Runtime, comentario en avacli/src/main.cpp).
    // Los imports dinamicos dentro de callbacks async siguen pasando por
    // los mismos hooks -- no hay ninguna ventana adicional aca.
    {
        while (raw_vm->HasPendingAsyncWork()) {
            raw_vm->PumpAsyncEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // La clave reconstruida ya cumplio su funcion para todo lo que hacia
    // falta descifrar en este arranque -- se borra de esta copia local
    // recien aca, porque los hooks de arriba (capturados por referencia
    // a `key`) pueden dispararse en cualquier punto hasta que la VM
    // termina de correr (imports dentro de funciones llamadas tarde,
    // callbacks async, etc.), no solo durante la compilacion inicial.
    std::memset(key, 0, sizeof(key));

    // El VM tiene que seguir vivo para el pump de arriba; el modulo se
    // destruye recien despues del VM por la misma razon que en
    // ava_barekernel_runner.cpp (evitar el use-after-free de teardown).
    ava_vm_destroy(vm);
    ava_module_destroy(module);
    return 0;
    // temp_guard se destruye aca (y en cualquier return de arriba) y borra
    // cualquier resto que haya quedado bajo temp_dir.
}
