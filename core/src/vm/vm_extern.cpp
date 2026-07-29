#include "vm_extern.h"

#include "value.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#if defined(AVA_HAVE_LIBFFI)
#include <ffi.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace ava {

namespace fs = std::filesystem;

namespace {

// Cache de librerías ya cargadas, por nombre lógico ("kernel32",
// "sqlite"). Nunca se descargan -- viven lo que vive el proceso, igual
// que el resto de los "prototipos" del compilador/VM.
std::mutex g_lib_mutex;
std::unordered_map<std::string, void*> g_loaded_libs;

// Nombre lógico -> candidatos de nombre de archivo por plataforma. Ver
// EXTERN_FFI_DESIGN.md, sección "Platform Resolution".
std::vector<std::string> CandidateFileNames(const std::string& logical) {
    std::vector<std::string> out;
    out.push_back(logical); // por si ya vino con extensión/nombre exacto
#if defined(_WIN32)
    out.push_back(logical + ".dll");
#elif defined(__APPLE__)
    out.push_back("lib" + logical + ".dylib");
    out.push_back(logical + ".dylib");
#else
    out.push_back("lib" + logical + ".so");
    out.push_back(logical + ".so");
#endif
    return out;
}

// Directorio del ejecutable actual (no el cwd -- alguien puede correr
// `ava_cli` desde cualquier lado). Usado para ubicar la carpeta
// `modules/` por defecto (ver ModulesRoot). Si falla por lo que sea,
// cae a current_path() -- peor es nada, sigue siendo un intento
// razonable de encontrar `modules/` al lado del binario.
fs::path ExecutableDir() {
    std::error_code ec;
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return fs::current_path(ec);
    return fs::path(std::string(buf, len)).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return fs::current_path(ec);
    auto real = fs::canonical(fs::path(buf), ec);
    return ec ? fs::path(buf).parent_path() : real.parent_path();
#else
    auto link = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return fs::current_path(ec);
    return link.parent_path();
#endif
}

// Carpeta por defecto donde buscar DLLs/.so/.dylib de módulos nativos,
// para no obligar a tirar todo suelto al lado del .exe: `modules/`
// junto al ejecutable (p.ej. `D:\_CODE_\avalang\build\Release\modules\`).
// Podés organizar ahí adentro como quieras -- una subcarpeta por
// librería (`modules\mysql\libmysql.dll`), todo junto, como sea --
// ver FindInModulesDir, que busca recursivo.
fs::path ModulesRoot() {
    return ExecutableDir() / "modules";
}

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Busca, recursivamente, un archivo cuyo nombre (sin importar mayúsculas
// en Windows) coincida con alguno de `filenames` dentro de `root` y sus
// subcarpetas. Primer match gana -- no valida duplicados entre
// subcarpetas a proposito (mantiene esto simple; si el usuario tiene
// dos "libmysql.dll" distintas en subcarpetas separadas, that's on them).
// Devuelve un path vacio si no encuentra nada.
fs::path FindInModulesDir(const fs::path& root, const std::vector<std::string>& filenames) {
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return {};

#if defined(_WIN32)
    std::vector<std::string> wanted;
    wanted.reserve(filenames.size());
    for (auto& f : filenames) wanted.push_back(ToLowerAscii(f));
#else
    const auto& wanted = filenames;
#endif

    fs::recursive_directory_iterator it(
        root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        std::error_code entry_ec;
        if (!it->is_regular_file(entry_ec) || entry_ec) continue;
        std::string name = it->path().filename().string();
#if defined(_WIN32)
        name = ToLowerAscii(name);
#endif
        if (std::find(wanted.begin(), wanted.end(), name) != wanted.end()) {
            return it->path();
        }
    }
    return {};
}

void* LoadFromPath(const fs::path& full_path) {
    if (full_path.empty()) return nullptr;
#if defined(_WIN32)
    return static_cast<void*>(LoadLibraryA(full_path.string().c_str()));
#else
    return dlopen(full_path.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
}

void* LoadNativeLibrary(const std::string& logical_name) {
    std::lock_guard<std::mutex> lock(g_lib_mutex);
    auto it = g_loaded_libs.find(logical_name);
    if (it != g_loaded_libs.end()) return it->second;

    auto candidates = CandidateFileNames(logical_name);

    // 1) Busqueda "de siempre": PATH, carpeta del ejecutable (Windows
    // ya la busca sola), rutas de sistema. Cubre libs del sistema
    // (kernel32, user32) sin tener que tocar nada.
    void* handle = nullptr;
    for (auto& candidate : candidates) {
        handle = LoadFromPath(candidate); // string relativo -> misma
                                            // resolucion que LoadLibraryA/
                                            // dlopen de siempre (fs::path
                                            // de un string relativo no
                                            // agrega nada).
        if (handle) break;
    }

    // 2) Si no aparecio, buscar dentro de modules/ al lado del .exe,
    // recursivo (subcarpetas). Pensado para no tener que copiar cada
    // DLL de tercero (libmysql.dll, sqlite3.dll, etc.) suelta al lado
    // del ejecutable -- las organizas en modules/<lo-que-quieras>/ y
    // las encuentra igual.
    if (!handle) {
        fs::path found = FindInModulesDir(ModulesRoot(), candidates);
        if (!found.empty()) handle = LoadFromPath(found);
    }

    g_loaded_libs[logical_name] = handle; // cachea el fallo también (nullptr)
    return handle;
}

void* ResolveSymbol(void* handle, const std::string& name) {
    if (!handle) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
}

std::string PlatformLoadError() {
#if defined(_WIN32)
    return "codigo de error Win32: " + std::to_string(GetLastError()) +
           " (revisar tambien " + ModulesRoot().string() + ")";
#else
    const char* err = dlerror();
    return (err ? std::string(err) : std::string("(sin detalle)")) +
           " (revisar tambien " + ModulesRoot().string() + ")";
#endif
}

} // namespace

extern "C" ava_value_t ava_extern_call(AvaVM*, const ava_value_t* c_args, size_t count, void* user_data) {
    auto* meta = static_cast<ExternFuncMeta*>(user_data);
    if (!meta) {
        throw std::runtime_error("extern: metadata de funcion nativa invalida");
    }
    const std::string where = meta->alias + "." + meta->func_name;

#if !defined(AVA_HAVE_LIBFFI)
    (void)c_args; (void)count;
    throw std::runtime_error(
        "extern: '" + where + "' no se puede invocar -- este build se compilo sin libffi. "
        "Instala libffi (p.ej. 'vcpkg install libffi') y reconfigura para habilitar llamadas "
        "nativas reales. Ver EXTERN_FFI_TODO.md.");
#else
    void* handle = LoadNativeLibrary(meta->library);
    if (!handle) {
        throw std::runtime_error(
            "extern: no se pudo cargar la libreria \"" + meta->library + "\" (para " + where +
            "). " + PlatformLoadError());
    }
    void* sym = ResolveSymbol(handle, meta->func_name);
    if (!sym) {
        throw std::runtime_error(
            "extern: simbolo \"" + meta->func_name + "\" no encontrado en la libreria \"" +
            meta->library + "\"");
    }

    std::vector<Value> args;
    args.reserve(count);
    for (size_t i = 0; i < count; ++i) args.push_back(FromC(c_args[i]));

    std::vector<ffi_type*> arg_types(count);
    std::vector<void*> arg_values(count);
    std::vector<long long> int_storage(count, 0);
    std::vector<double> dbl_storage(count, 0.0);
    std::vector<const char*> str_storage(count, nullptr);
    std::vector<void*> ptr_storage(count, nullptr);

    for (size_t i = 0; i < count; ++i) {
        const Value& v = args[i];
        switch (v.type) {
            case ValueType::String: {
                str_storage[i] = static_cast<StringObj*>(v.obj)->data.c_str();
                arg_types[i] = &ffi_type_pointer;
                arg_values[i] = &str_storage[i];
                break;
            }
            case ValueType::Number: {
                double d = v.n;
                if (d == static_cast<double>(static_cast<long long>(d))) {
                    int_storage[i] = static_cast<long long>(d);
                    arg_types[i] = &ffi_type_sint64;
                    arg_values[i] = &int_storage[i];
                } else {
                    dbl_storage[i] = d;
                    arg_types[i] = &ffi_type_double;
                    arg_values[i] = &dbl_storage[i];
                }
                break;
            }
            case ValueType::Bool: {
                int_storage[i] = v.b ? 1 : 0;
                arg_types[i] = &ffi_type_sint64;
                arg_values[i] = &int_storage[i];
                break;
            }
            case ValueType::Nil: {
                ptr_storage[i] = nullptr;
                arg_types[i] = &ffi_type_pointer;
                arg_values[i] = &ptr_storage[i];
                break;
            }
            default:
                throw std::runtime_error(
                    "extern: tipo de argumento no soportado llamando a " + where +
                    " (solo number/string/bool/nil por ahora)");
        }
    }

    ffi_cif cif;
    ffi_type* ret_type = &ffi_type_sint64; // ver limitaciones en vm_extern.h
    ffi_status status = ffi_prep_cif(
        &cif, FFI_DEFAULT_ABI, static_cast<unsigned int>(count), ret_type, arg_types.data());
    if (status != FFI_OK) {
        throw std::runtime_error("extern: ffi_prep_cif fallo para " + where);
    }

    long long result = 0;
    ffi_call(&cif, FFI_FN(sym), &result, count > 0 ? arg_values.data() : nullptr);

    return ToC(Value::Number(static_cast<double>(result)));
#endif
}

} // namespace ava
