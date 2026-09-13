#include "vm_extern.h"
#include "vm_platform_accessor.h"

#include "value.h"

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "../../platform/barekernel/BareKernelCaps.h"

#if AVA_HAVE_STD_LIBRARY

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

#if defined(AVA_HAVE_LIBFFI)
#include <ffi.h>
#endif

#if defined(_WIN32) && defined(AVA_HAVE_LIBFFI)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace ava {

namespace fs = std::filesystem;

namespace {

// Cache de librerías ya cargadas, por nombre lógico ("kernel32",
// "sqlite"). Nunca se descargan -- viven lo que vive el proceso, igual
// que el resto de los "prototipos" del compilador/VM.
avastd::mutex g_lib_mutex;
avastd::unordered_map<avastd::string, platform::ILibraryHandle*> g_loaded_libs;

// Nombre lógico -> candidatos de nombre de archivo por plataforma
// (sección "Platform Resolution").
avastd::vector<avastd::string> CandidateFileNames(const avastd::string& logical) {
    avastd::vector<avastd::string> out;
    out.push_back(logical); // por si ya vino con extensión/nombre exacto
#if defined(_WIN32)
    out.push_back(logical + ".dll");
#elif defined(__APPLE__)
    out.push_back("lib" + logical + ".dylib");
    out.push_back(logical + ".dylib");
#else
    out.push_back("lib" + logical + ".so");
    out.push_back(logical + ".so");
    // En muchas distros Linux, libfoo.so es un linker script (texto),
    // no un ELF -- dlopen no lo carga. El ELF real tiene versión
    // (libfoo.so.6). Añadimos los sufijos versionados comunes como
    // último recurso. Si la lib está en ldconfig, dlopen la encuentra
    // por nombre versionado.
    out.push_back("lib" + logical + ".so.6");
    out.push_back("lib" + logical + ".so.5");
    out.push_back("lib" + logical + ".so.5d");
    out.push_back("lib" + logical + ".so.4");
    out.push_back("lib" + logical + ".so.3");
    out.push_back("lib" + logical + ".so.2");
    out.push_back("lib" + logical + ".so.1");
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
    avastd::string exe_dir = VmPlatformAccessor::Get().FileSystem().GetExecutableDirectory();
    if (exe_dir.empty()) {
        return fs::current_path(ec);
    }
    return fs::path(exe_dir);
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

avastd::string ToLowerAscii(avastd::string s) {
    avastd::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Busca, recursivamente, un archivo cuyo nombre (sin importar mayúsculas
// en Windows) coincida con alguno de `filenames` dentro de `root` y sus
// subcarpetas. Primer match gana -- no valida duplicados entre
// subcarpetas a proposito (mantiene esto simple; si el usuario tiene
// dos "libmysql.dll" distintas en subcarpetas separadas, that's on them).
// Devuelve un path vacio si no encuentra nada.
fs::path FindInModulesDir(const fs::path& root, const avastd::vector<avastd::string>& filenames) {
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return {};

#if defined(_WIN32)
    avastd::vector<avastd::string> wanted;
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
        avastd::string name = it->path().filename().string();
#if defined(_WIN32)
        name = ToLowerAscii(name);
#endif
        if (avastd::find(wanted.begin(), wanted.end(), name) != wanted.end()) {
            return it->path();
        }
    }
    return {};
}

// Load a library from an absolute path. Returns an opaque handle managed
// by the PAL's ILibraryLoader. Caller must track and eventually Unload() it.
platform::ILibraryHandle* LoadFromPath(const fs::path& full_path) {
    if (full_path.empty()) return nullptr;
    return VmPlatformAccessor::Get().Libraries().Load(full_path.string());
}

platform::ILibraryHandle* LoadNativeLibrary(const avastd::string& logical_name) {
    avastd::lock_guard<avastd::mutex> lock(g_lib_mutex);
    auto it = g_loaded_libs.find(logical_name);
    if (it != g_loaded_libs.end()) return it->second;

    auto candidates = CandidateFileNames(logical_name);

    // 1) Busqueda "de siempre": PATH, carpeta del ejecutable (Windows
    // ya la busca sola), rutas de sistema. Cubre libs del sistema
    // (kernel32, user32) sin tener que tocar nada.
    platform::ILibraryHandle* handle = nullptr;
    for (auto& candidate : candidates) {
        handle = LoadFromPath(candidate); // string relativo -> misma
                                            // resolucion que LoadLibraryA/
                                            // dlopen de siempre
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

void* ResolveSymbol(platform::ILibraryHandle* handle, const avastd::string& name) {
    if (!handle) return nullptr;
    return handle->ResolveSymbol(name);
}

avastd::string PlatformLoadError() {
    // Error details now come from the platform's ILibraryLoader implementation.
    // For better error messages, ILibraryLoader could be extended with error()
    // method in Phase 5+. For now, we provide a generic message with the modules
    // search path.
    return "(ver tambien " + ModulesRoot().string() + " para modulos nativos personalizados)";
}

#if defined(_WIN32) && defined(AVA_HAVE_LIBFFI)
// A bad argument/return marshaling in a native call (wrong arg count,
// wrong type width, wrong calling convention) can make libffi read or
// write outside valid memory inside the target DLL. That raises a
// Windows structured exception (access violation), not a C++
// exception -- it never reaches a try/catch and kills the whole
// process before anything gets to print. This turns that crash into a
// normal AvaLang error instead, naming the call that failed.
//
// __try/__except cannot share a function with C++ objects that need
// unwinding (MSVC error C2712), so this stays free of std::string/
// std::vector/etc. and only takes raw pointers.
DWORD SehFilter(EXCEPTION_POINTERS* info, DWORD* out_code) {
    if (out_code && info && info->ExceptionRecord) {
        *out_code = info->ExceptionRecord->ExceptionCode;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

bool CallNativeGuarded(ffi_cif* cif, void* fn, void* ret, void** arg_values, DWORD* exception_code) {
    __try {
        ffi_call(cif, FFI_FN(fn), ret, arg_values);
        return true;
    } __except (SehFilter(GetExceptionInformation(), exception_code)) {
        return false;
    }
}
#endif

} // namespace

extern "C" ava_value_t ava_extern_call(AvaVM*, const ava_value_t* c_args, size_t count, void* user_data) {
    auto* meta = static_cast<ExternFuncMeta*>(user_data);
    if (!meta) {
        throw avastd::runtime_error("extern: metadata de funcion nativa invalida");
    }
    const avastd::string where = meta->alias + "." + meta->func_name;

#if !defined(AVA_HAVE_LIBFFI)
    (void)c_args; (void)count;
    throw avastd::runtime_error(
        "extern: '" + where + "' no se puede invocar -- este build se compilo sin libffi. "
        "Instala libffi (p.ej. 'vcpkg install libffi') y reconfigura para habilitar llamadas "
        "nativas reales.");
#else
    platform::ILibraryHandle* handle = LoadNativeLibrary(meta->library);
    if (!handle) {
        throw avastd::runtime_error(
            "extern: no se pudo cargar la libreria \"" + meta->library + "\" (para " + where +
            "). " + PlatformLoadError());
    }
    void* sym = ResolveSymbol(handle, meta->func_name);
    if (!sym) {
        throw avastd::runtime_error(
            "extern: simbolo \"" + meta->func_name + "\" no encontrado en la libreria \"" +
            meta->library + "\"");
    }

    avastd::vector<Value> args;
    args.reserve(count);
    for (size_t i = 0; i < count; ++i) args.push_back(FromC(c_args[i]));

    avastd::vector<ffi_type*> arg_types(count);
    avastd::vector<void*> arg_values(count);
    avastd::vector<long long> int_storage(count, 0);
    avastd::vector<double> dbl_storage(count, 0.0);
    avastd::vector<const char*> str_storage(count, nullptr);
    avastd::vector<void*> ptr_storage(count, nullptr);

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
                throw avastd::runtime_error(
                    "extern: tipo de argumento no soportado llamando a " + where +
                    " (solo number/string/bool/nil por ahora)");
        }
    }

    ffi_cif cif;
    // ret_type = pointer: en x64 Windows, puntero e int64 ambos viajan en
    // RAX (8 bytes), pero declarar el retorno como `pointer` es semántica-
    // mente correcto para funciones como mysql_init/mysql_store_result/
    // mysql_fetch_row/mysql_fetch_field que devuelven punteros. Antes usaba-
    // mos sint64 para todo, que funcionaba por casualidad en x64 pero era
    // fragile en otras ABIs y dejaba el código inconsistente con la reali-
    // dad del FFI. Para retornos int (mysql_query, mysql_errno, etc.) esto
    // sigue siendo correcto: un int de 8 bytes cabe exacto en el espacio
    // de un puntero en x64.
    ffi_type* ret_type = &ffi_type_pointer; // ver limitaciones en vm_extern.h
    ffi_status status = ffi_prep_cif(
        &cif, FFI_DEFAULT_ABI, static_cast<unsigned int>(count), ret_type, arg_types.data());
    if (status != FFI_OK) {
        throw avastd::runtime_error("extern: ffi_prep_cif fallo para " + where);
    }

    long long result = 0;
    void** ffi_args = count > 0 ? arg_values.data() : nullptr;

#if defined(_WIN32)
    DWORD exception_code = 0;
    if (!CallNativeGuarded(&cif, sym, &result, ffi_args, &exception_code)) {
        char code_buf[16];
        avastd::snprintf(code_buf, sizeof(code_buf), "0x%08lX", static_cast<unsigned long>(exception_code));
        throw avastd::runtime_error(
            "extern: '" + where + "' crashed the native call (exception " + code_buf +
            "). This usually means the extern signature doesn't match the real C "
            "function (argument count/order/type) -- check it against the library's header.");
    }
#else
    ffi_call(&cif, FFI_FN(sym), &result, ffi_args);
#endif

    return ToC(Value::Number(static_cast<double>(result)));
#endif
}

} // namespace ava

#elif defined(AVA_TARGET_BAREKERNEL) && CKM_CAP_DYNAMIC_LOADING

#include "../../platform/barekernel/BareKernelExternThunk.h"

namespace ava {

namespace {

avastd::mutex g_lib_mutex;
avastd::unordered_map<avastd::string, platform::ILibraryHandle*> g_loaded_libs;

avastd::vector<avastd::string> CandidateFileNames(const avastd::string& logical) {
    avastd::vector<avastd::string> out;
    out.push_back(logical);
    out.push_back(logical + ".so");
    out.push_back("lib" + logical + ".so");
    return out;
}

avastd::string JoinPath(const avastd::string& dir, const avastd::string& name) {
    if (dir.empty()) return name;
    if (dir[dir.size() - 1] == '/') return dir + name;
    return dir + "/" + name;
}

avastd::string ModulesRoot() {
    avastd::string exe_dir = VmPlatformAccessor::Get().FileSystem().GetExecutableDirectory();
    if (exe_dir.empty()) exe_dir = "/";
    return JoinPath(exe_dir, "modules");
}

bool NameMatches(const avastd::string& filename, const avastd::vector<avastd::string>& wanted) {
    for (avastd::size_t i = 0; i < wanted.size(); ++i) {
        if (filename == wanted[i]) return true;
    }
    return false;
}

avastd::string FindInModulesDir(const avastd::string& root, const avastd::vector<avastd::string>& filenames) {
    platform::IFileSystem& fs = VmPlatformAccessor::Get().FileSystem();
    if (!fs.Exists(root) || !fs.IsDirectory(root)) return avastd::string();

    avastd::vector<platform::DirEntry> entries;
    if (!fs.EnumerateDirectory(root, entries)) return avastd::string();

    for (avastd::size_t i = 0; i < entries.size(); ++i) {
        const platform::DirEntry& e = entries[i];
        avastd::string full = JoinPath(root, e.name);
        if (e.is_directory) {
            avastd::string found = FindInModulesDir(full, filenames);
            if (!found.empty()) return found;
        } else if (NameMatches(e.name, filenames)) {
            return full;
        }
    }
    return avastd::string();
}

platform::ILibraryHandle* LoadFromPath(const avastd::string& path) {
    if (path.empty()) return nullptr;
    return VmPlatformAccessor::Get().Libraries().Load(path);
}

platform::ILibraryHandle* LoadNativeLibrary(const avastd::string& logical_name) {
    avastd::lock_guard<avastd::mutex> lock(g_lib_mutex);
    auto it = g_loaded_libs.find(logical_name);
    if (it != g_loaded_libs.end()) return it->second;

    avastd::vector<avastd::string> candidates = CandidateFileNames(logical_name);

    platform::ILibraryHandle* handle = nullptr;
    for (avastd::size_t i = 0; i < candidates.size() && !handle; ++i) {
        handle = LoadFromPath(candidates[i]);
    }
    if (!handle) {
        avastd::string found = FindInModulesDir(ModulesRoot(), candidates);
        if (!found.empty()) handle = LoadFromPath(found);
    }

    g_loaded_libs[logical_name] = handle;
    return handle;
}

void* ResolveSymbol(platform::ILibraryHandle* handle, const avastd::string& name) {
    if (!handle) return nullptr;
    return handle->ResolveSymbol(name);
}

avastd::string PlatformLoadError() {
    return "(ver tambien " + ModulesRoot() + " para modulos nativos personalizados)";
}

} // namespace

extern "C" ava_value_t ava_extern_call(AvaVM*, const ava_value_t* c_args, size_t count, void* user_data) {
    auto* meta = static_cast<ExternFuncMeta*>(user_data);
    if (!meta) {
        AVA_THROW(avastd::runtime_error("extern: metadata de funcion nativa invalida"));
    }
    const avastd::string where = meta->alias + "." + meta->func_name;

    platform::ILibraryHandle* handle = LoadNativeLibrary(meta->library);
    if (!handle) {
        AVA_THROW(avastd::runtime_error(
            "extern: no se pudo cargar la libreria \"" + meta->library + "\" (para " + where +
            "). " + PlatformLoadError()));
    }
    void* sym = ResolveSymbol(handle, meta->func_name);
    if (!sym) {
        AVA_THROW(avastd::runtime_error(
            "extern: simbolo \"" + meta->func_name + "\" no encontrado en la libreria \"" +
            meta->library + "\""));
    }

    avastd::vector<platform::barekernel::ExternArg> thunk_args;
    thunk_args.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        Value v = FromC(c_args[i]);
        platform::barekernel::ExternArg arg{};
        switch (v.type) {
            case ValueType::String: {
                arg.kind = platform::barekernel::ExternArgKind::Pointer;
                arg.ptr = const_cast<char*>(static_cast<StringObj*>(v.obj)->data.c_str());
                break;
            }
            case ValueType::Number: {
                double d = v.n;
                if (d == static_cast<double>(static_cast<avastd::int32_t>(d))) {
                    arg.kind = platform::barekernel::ExternArgKind::Int32;
                    arg.i32 = static_cast<avastd::int32_t>(d);
                } else if (d == static_cast<double>(static_cast<avastd::int64_t>(d))) {
                    arg.kind = platform::barekernel::ExternArgKind::Int64;
                    arg.i64 = static_cast<avastd::int64_t>(d);
                } else {
                    arg.kind = platform::barekernel::ExternArgKind::Double;
                    arg.f64 = d;
                }
                break;
            }
            case ValueType::Bool: {
                arg.kind = platform::barekernel::ExternArgKind::Int32;
                arg.i32 = v.b ? 1 : 0;
                break;
            }
            case ValueType::Nil: {
                arg.kind = platform::barekernel::ExternArgKind::Pointer;
                arg.ptr = nullptr;
                break;
            }
            default:
                AVA_THROW(avastd::runtime_error(
                    "extern: tipo de argumento no soportado llamando a " + where +
                    " (solo number/string/bool/nil por ahora)"));
        }
        thunk_args.push_back(arg);
    }

    avastd::int32_t result = platform::barekernel::CallExternCdecl(sym, thunk_args);
    return ToC(Value::Number(static_cast<double>(result)));
}

} // namespace ava

#else

namespace ava {

extern "C" ava_value_t ava_extern_call(AvaVM*, const ava_value_t*, size_t, void*) {
    AVA_THROW(avastd::runtime_error(
        "extern: bloques 'extern' (FFI a librerias nativas) no estan "
        "soportados en este build -- ver nota en vm_extern.cpp"));
#if !AVA_HAVE_EXCEPTIONS
    ava_value_t dummy{};
    return dummy;
#endif
}

} // namespace ava

#endif  // AVA_HAVE_STD_LIBRARY