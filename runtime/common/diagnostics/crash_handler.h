#pragma once
// Fase 1 de PLAN_DEBUG_MODE_AVASTUDIO.md: handler de excepcion no manejada
// compartido por TODOS los runners nativos (ava_cli, avapack_stub,
// avapack_stub_ui, el .exe con-repo de avapack, avanative.exe) -- no solo
// avanative.exe, que era el unico que lo tenia hasta ahora. Extraido de la
// implementacion original en runtime/avahost/src/native/main_native.cpp
// (WriteCrashDumpAndNotify) sin cambiar su comportamiento: SEH +
// MiniDumpWriteDump + MessageBoxA, ahora reusable desde un solo lugar.
//
// Por que esto importa: sin este filtro, un exe compilado por Avalang que
// corre bajo subsystem WINDOWS (sin consola -- avanative.exe, cualquier
// proyecto --with-ui) muere en una excepcion no manejada (access violation,
// stack overflow, etc.) SIN dialogo de Windows ni forma de saber que paso.
// Y aunque el binario tenga consola (avapack_stub.exe, ava_cli.exe), el
// default de Windows Error Reporting no le sirve de nada al usuario de
// Avalang: no dice nada sobre el .ava que estaba corriendo.
//
// Solo Windows por ahora (SEH + dbghelp.lib). El backend POSIX (sigaction +
// backtrace(), mismo patron que usa Breakpad/Crashpad en Linux/macOS) queda
// pendiente para cuando haga falta soportar esos hosts -- ver Fase 1 del
// plan. Mientras tanto, InstallCrashHandler(...) es un no-op fuera de
// Windows para que el codigo que lo llama compile igual en todas partes.
//
// Header-only a proposito, igual que error_report.h -- instalar el handler
// es una linea (ava::diag::InstallCrashHandler("nombre_del_runner")) al
// principio de main(), sin agregar ninguna lib/target nueva a wirear en
// los CMakeLists.txt existentes.
#include <string>

#include "diagnostics/error_report.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace ava {
namespace diag {

namespace detail {

// SetUnhandledExceptionFilter no admite closures/lambdas con captura -- es
// un puntero a funcion C puro. El nombre del runner (para el .dmp y el
// titulo del MessageBox) se guarda aca para que el filtro instalado lo
// pueda leer. `inline` variable (C++17+) para que este header se pueda
// incluir desde mas de una translation unit sin "multiple definition".
inline std::string& CrashHandlerAppName() {
    static std::string name = "avalang";
    return name;
}

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

// Carpeta previsible y con permisos de escritura garantizados para volcar
// .dmp, sin depender de que la carpeta del .exe sea escribible (Program
// Files no lo es sin elevar): %TEMP%/avalang/crashes/. CreateDirectoryW no
// falla si la carpeta ya existe, asi que no hace falta chequear antes.
inline std::wstring CrashDumpDir() {
    wchar_t temp_path[MAX_PATH] = {};
    DWORD len = GetTempPathW(MAX_PATH, temp_path);
    std::wstring base = (len > 0 && len < MAX_PATH) ? std::wstring(temp_path) : L".\\";
    std::wstring avalang_dir = base + L"avalang\\";
    std::wstring crashes_dir = avalang_dir + L"crashes\\";
    CreateDirectoryW(avalang_dir.c_str(), nullptr);
    CreateDirectoryW(crashes_dir.c_str(), nullptr);
    return crashes_dir;
}

// ---------------------------------------------------------------------
// Todo lo que sigue corre DENTRO del filtro de excepcion no manejada, es
// decir, con el proceso ya en un estado inconsistente (heap posiblemente
// corrupto, stack casi agotado). Por eso se evita el heap (std::string,
// ostringstream) y se trabaja con buffers de tamano fijo: si el handler
// mismo vuelve a fallar, el proceso muere en silencio -- justo lo que este
// handler existe para evitar.
// ---------------------------------------------------------------------

constexpr size_t kCrashPathCap = MAX_PATH + 64;

inline void AppendF(char* buf, size_t cap, size_t& used, const char* fmt, ...) {
    if (used + 1 >= cap) return;
    va_list args;
    va_start(args, fmt);
    const int n = std::vsnprintf(buf + used, cap - used, fmt, args);
    va_end(args);
    if (n < 0) return;
    const size_t avail = cap - used - 1;
    used += (static_cast<size_t>(n) < avail) ? static_cast<size_t>(n) : avail;
}

inline const char* ExceptionCodeName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
        case 0xE06D7363:                      return "C++ exception (no capturada)";
        default:                              return "desconocida";
    }
}

// Carga simbolos (SYMOPT_LOAD_LINES) para el proceso UNA sola vez, llamado
// desde InstallCrashHandler mientras el proceso todavia esta sano -- asi
// el trabajo pesado (enumerar modulos, abrir .pdb) no pasa dentro del
// filtro de excepcion. Con NULL como search path, dbghelp ya busca en el
// directorio de cada modulo cargado (ademas de cwd y _NT_SYMBOL_PATH), que
// es donde vive el .pdb de cada .dll/.exe en un build normal.
inline void EnsureSymbolsLoaded() {
    static bool done = false;
    if (done) return;
    done = true;
    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

// "avalang.dll+0x1a2b3c" para una direccion; si no cae en ningun modulo
// cargado lo dice explicitamente (salto a memoria basura / puntero nulo).
inline void DescribeAddress(const void* addr, char* out, size_t cap) {
    HMODULE module = nullptr;
    if (addr != nullptr &&
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addr), &module) &&
        module != nullptr) {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(module, path, MAX_PATH);
        const char* name = std::strrchr(path, '\\');
        name = name ? name + 1 : path;
        const unsigned long long offset = static_cast<unsigned long long>(
            reinterpret_cast<const char*>(addr) - reinterpret_cast<const char*>(module));
        std::snprintf(out, cap, "%s+0x%llx", name, offset);
    } else {
        std::snprintf(out, cap, "0x%p (fuera de todo modulo cargado)", addr);
    }
    out[cap - 1] = '\0';
}

// Simbolos (nombre de funcion + archivo:linea) via dbghelp, ademas del
// modulo+offset de siempre. SymInitialize/SymSetOptions se llaman UNA vez
// desde InstallCrashHandler (proceso todavia sano, no desde dentro del
// filtro de excepcion) -- aca solo se consultan simbolos ya cargados, con
// buffers de tamano fijo (nada de heap). Requiere que el .pdb del modulo
// este en su misma carpeta (o en el path de simbolos default de dbghelp);
// ver AVA_ENABLE_CRASH_SYMBOLS en CMakeLists.txt / OptimizationOptions.cmake
// -- sin esa opcion (o en un modulo de terceros sin .pdb) esto no encuentra
// nada y el frame se queda solo con modulo+offset, que es el fallback de
// siempre. `is_return_address` en true resta 1 antes de resolver: los
// frames que no son el #0 son direcciones de retorno (la instruccion
// DESPUES del call), no el call en si -- resolverlas tal cual apuntaria a
// la linea siguiente.
//
// NOTA: esto da archivo y NUMERO DE LINEA, pero no columna -- las tablas
// de linea clasicas de MSVC (IMAGEHLP_LINE64) no tienen ese dato; conseguir
// columna requeriria el SDK de DIA (msdia*.dll via COM), bastante mas
// pesado que dbghelp. Si hace falta columna real, avisar para agregarlo.
inline void AppendSymbolInfo(DWORD64 pc, bool is_return_address, char* buf, size_t cap, size_t& used) {
    if (pc == 0) return;
    const DWORD64 lookup_pc = pc - (is_return_address ? 1 : 0);
    const HANDLE process = GetCurrentProcess();

    alignas(SYMBOL_INFO) char sym_storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(sym_storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 name_displacement = 0;
    const bool have_name = SymFromAddr(process, lookup_pc, &name_displacement, symbol) != FALSE;

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    DWORD line_displacement = 0;
    const bool have_line = SymGetLineFromAddr64(process, lookup_pc, &line_displacement, &line) != FALSE;

    if (have_name) AppendF(buf, cap, used, "  %s", symbol->Name);
    if (have_line) AppendF(buf, cap, used, "  (%s:%lu)", line.FileName, static_cast<unsigned long>(line.LineNumber));
}

#if defined(_MSC_VER) && defined(_M_X64)
// Recorre la pila del hilo que fallo a partir del CONTEXT de la excepcion
// (no de la pila del handler) usando la info de unwind de cada modulo --
// eso no necesita simbolos. Ademas de modulo+offset (siempre disponible),
// cada frame intenta agregar funcion + archivo:linea via AppendSymbolInfo
// (ver arriba) cuando el .pdb correspondiente esta disponible. Un __try
// protege contra una pila corrupta.
inline void AppendStackTrace(char* buf, size_t cap, size_t& used, const CONTEXT* context) {
    CONTEXT ctx = *context;
    __try {
        for (int i = 0; i < 24; ++i) {
            const DWORD64 pc = ctx.Rip;
            if (pc == 0 && i > 0) break;

            char where[kCrashPathCap];
            DescribeAddress(reinterpret_cast<const void*>(pc), where, sizeof(where));
            AppendF(buf, cap, used, "  #%d %s", i, where);
            AppendSymbolInfo(pc, /*is_return_address=*/i > 0, buf, cap, used);
            AppendF(buf, cap, used, "\n");

            DWORD64 image_base = 0;
            PRUNTIME_FUNCTION function = pc ? RtlLookupFunctionEntry(pc, &image_base, nullptr) : nullptr;
            if (function == nullptr) {
                if (ctx.Rsp == 0) break;
                ctx.Rip = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
                ctx.Rsp += 8;
            } else {
                PVOID handler_data = nullptr;
                DWORD64 establisher_frame = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, pc, function, &ctx, &handler_data,
                                 &establisher_frame, nullptr);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        AppendF(buf, cap, used, "  (pila ilegible: el recorrido fallo)\n");
    }
}
#endif

inline void WideToUtf8(const wchar_t* src, char* dst, size_t cap) {
    dst[0] = '\0';
    if (cap == 0) return;
    if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, static_cast<int>(cap), nullptr, nullptr) <= 0) {
        dst[0] = '\0';
    }
    dst[cap - 1] = '\0';
}

// %TEMP%/avalang/crashes/<app>_<timestamp>.{dmp,txt} -- mismo esquema de
// nombres que antes (unico por timestamp, nunca sobreescribe).
inline void BuildCrashFilePaths(const char* app_name, const SYSTEMTIME& now, wchar_t* dump_path,
                                wchar_t* report_path, size_t cap) {
    wchar_t temp[MAX_PATH] = {};
    const DWORD len = GetTempPathW(MAX_PATH, temp);
    wchar_t base[kCrashPathCap] = {};
    if (len > 0 && len < MAX_PATH) {
        _snwprintf(base, kCrashPathCap - 1, L"%savalang\\", temp);
    } else {
        _snwprintf(base, kCrashPathCap - 1, L".\\avalang\\");
    }
    CreateDirectoryW(base, nullptr);

    wchar_t crashes[kCrashPathCap] = {};
    _snwprintf(crashes, kCrashPathCap - 1, L"%scrashes\\", base);
    CreateDirectoryW(crashes, nullptr);

    wchar_t app_w[64] = {};
    if (MultiByteToWideChar(CP_UTF8, 0, app_name, -1, app_w, 63) <= 0 || app_w[0] == L'\0') {
        _snwprintf(app_w, 63, L"avalang");
    }

    _snwprintf(dump_path, cap - 1, L"%s%s_%04u%02u%02u_%02u%02u%02u.dmp", crashes, app_w, now.wYear,
               now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    _snwprintf(report_path, cap - 1, L"%s%s_%04u%02u%02u_%02u%02u%02u.txt", crashes, app_w, now.wYear,
               now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    dump_path[cap - 1] = L'\0';
    report_path[cap - 1] = L'\0';
}

inline bool WriteBufferToFile(const wchar_t* path, const char* data, size_t size) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(file);
    return ok != FALSE;
}

inline bool WriteMiniDump(const wchar_t* path, EXCEPTION_POINTERS* exception_info, DWORD crashed_thread_id,
                          MINIDUMP_TYPE type) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    MINIDUMP_EXCEPTION_INFORMATION mdei{};
    mdei.ThreadId = crashed_thread_id;
    mdei.ExceptionPointers = exception_info;
    mdei.ClientPointers = FALSE;
    const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                                       exception_info ? &mdei : nullptr, nullptr, nullptr);
    CloseHandle(file);
    return ok != FALSE;
}

// Misma linea "@@AVA_ERROR@@ {json}" que error_report.h::EmitStructuredError
// (kind = native_crash), pero armada en un buffer fijo y escrita con
// WriteFile directo al handle de stderr: no toca el heap ni el CRT. Es lo
// que AvaStudio parsea para mostrar el crash en el panel de Logs.
inline void WriteStructuredCrashLine(const char* message) {
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err == nullptr || err == INVALID_HANDLE_VALUE) return;

    char line[8192];
    size_t n = 0;
    const char kHead[] = "@@AVA_ERROR@@ {\"kind\":\"native_crash\",\"message\":\"";
    std::memcpy(line, kHead, sizeof(kHead) - 1);
    n = sizeof(kHead) - 1;

    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(message);
         *p != 0 && n + 8 < sizeof(line) - 4; ++p) {
        switch (*p) {
            case '"':  line[n++] = '\\'; line[n++] = '"';  break;
            case '\\': line[n++] = '\\'; line[n++] = '\\'; break;
            case '\n': line[n++] = '\\'; line[n++] = 'n';  break;
            case '\r': line[n++] = '\\'; line[n++] = 'r';  break;
            case '\t': line[n++] = '\\'; line[n++] = 't';  break;
            default:
                if (*p < 0x20) {
                    n += static_cast<size_t>(std::snprintf(line + n, sizeof(line) - n, "\\u%04x", *p));
                } else {
                    line[n++] = static_cast<char>(*p);
                }
        }
    }
    line[n++] = '"';
    line[n++] = '}';
    line[n++] = '\n';

    DWORD written = 0;
    WriteFile(err, line, static_cast<DWORD>(n), &written, nullptr);
}

// True when a parent process is reading our stderr through a pipe (AvaStudio's
// Build/Run panel, a CI job, a wrapper script). That parent already receives
// the structured "@@AVA_ERROR@@" line written by WriteStructuredCrashLine and
// shows it in its own log, so a modal MessageBox on top of it is redundant --
// and it blocks the dying process until someone clicks it.
//
// Standalone runs (double click, no pipe on stderr) still get the dialog:
// there nobody else would tell the user the app died.
//
// Only Win32 calls that do not allocate, so it is safe inside the exception
// filter.
inline bool StderrIsCapturedByParent() {
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err == nullptr || err == INVALID_HANDLE_VALUE) return false;
    return GetFileType(err) == FILE_TYPE_PIPE;
}

inline void ReportCrash(EXCEPTION_POINTERS* exception_info, DWORD crashed_thread_id) {
    const char* app_name = CrashHandlerAppName().c_str();
    SYSTEMTIME now;
    GetLocalTime(&now);

    wchar_t dump_path_w[kCrashPathCap] = {};
    wchar_t report_path_w[kCrashPathCap] = {};
    BuildCrashFilePaths(app_name, now, dump_path_w, report_path_w, kCrashPathCap);
    char dump_path[kCrashPathCap * 3] = {};
    char report_path[kCrashPathCap * 3] = {};
    WideToUtf8(dump_path_w, dump_path, sizeof(dump_path));
    WideToUtf8(report_path_w, report_path, sizeof(report_path));

    // 1) Informe legible. Las lineas "Codigo: " y "Direccion: " conservan su
    //    formato de siempre: AvaStudio las extrae (ExtractHexField). Buffer
    //    mas grande que antes (era 6144) porque cada frame ahora puede
    //    sumar nombre de funcion + archivo:linea via AppendSymbolInfo, y
    //    nombres de C++ (templates/lambdas de la STL) pueden ser largos.
    char report[16384];
    size_t used = 0;
    report[0] = '\0';
    const EXCEPTION_RECORD* record = exception_info ? exception_info->ExceptionRecord : nullptr;
    AppendF(report, sizeof(report), used, "%s crasheo con una excepcion no manejada.\n\n", app_name);
    if (record != nullptr) {
        AppendF(report, sizeof(report), used, "Codigo: 0x%lx (%s)\n",
                static_cast<unsigned long>(record->ExceptionCode), ExceptionCodeName(record->ExceptionCode));
        AppendF(report, sizeof(report), used, "Direccion: 0x%p\n", record->ExceptionAddress);

        char where[kCrashPathCap];
        DescribeAddress(record->ExceptionAddress, where, sizeof(where));
        AppendF(report, sizeof(report), used, "Modulo: %s", where);
        AppendSymbolInfo(reinterpret_cast<DWORD64>(record->ExceptionAddress), /*is_return_address=*/false, report,
                         sizeof(report), used);
        AppendF(report, sizeof(report), used, "\n");

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const ULONG_PTR kind = record->ExceptionInformation[0];
            AppendF(report, sizeof(report), used, "Acceso: %s de la direccion 0x%p\n",
                    kind == 0 ? "lectura" : (kind == 1 ? "escritura" : (kind == 8 ? "ejecucion (DEP)" : "desconocido")),
                    reinterpret_cast<void*>(record->ExceptionInformation[1]));
        }
    } else {
        AppendF(report, sizeof(report), used, "Codigo: (sin registro de excepcion)\n");
    }
    AppendF(report, sizeof(report), used, "Hilo: %lu\n", static_cast<unsigned long>(crashed_thread_id));

#if defined(_MSC_VER) && defined(_M_X64)
    if (exception_info != nullptr && exception_info->ContextRecord != nullptr) {
        AppendF(report, sizeof(report), used, "\nPila (modulo+offset):\n");
        AppendStackTrace(report, sizeof(report), used, exception_info->ContextRecord);
    }
#endif
    AppendF(report, sizeof(report), used, "\nInforme: %s\nVolcado (.dmp, si se pudo escribir): %s\n",
            report_path, dump_path);

    // 2) Texto a disco y linea estructurada a stderr ANTES del volcado: el
    //    volcado con memoria completa puede tardar (o fallar); lo importante
    //    ya quedo dicho para entonces.
    WriteBufferToFile(report_path_w, report, used);
    WriteStructuredCrashLine(report);

    // 3) Volcado. Memoria completa como siempre; si no se puede, uno chico.
    bool dump_written = WriteMiniDump(dump_path_w, exception_info, crashed_thread_id, MiniDumpWithFullMemory);
    if (!dump_written) {
        dump_written = WriteMiniDump(
            dump_path_w, exception_info, crashed_thread_id,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo |
                                        MiniDumpWithUnloadedModules));
    }
    AppendF(report, sizeof(report), used, "%s\n",
            dump_written ? "Volcado (.dmp) escrito correctamente." : "No se pudo escribir el volcado (.dmp).");

    // The report file, the .dmp and the structured stderr line are already
    // written. If a parent captures stderr it shows the crash in its own log,
    // so skip the modal (see StderrIsCapturedByParent above).
    if (StderrIsCapturedByParent()) return;

    char title[160];
    std::snprintf(title, sizeof(title), "%s - excepcion no manejada", app_name);
    MessageBoxA(nullptr, report, title, MB_OK | MB_ICONERROR);
}

struct CrashReportRequest {
    EXCEPTION_POINTERS* exception_info;
    DWORD crashed_thread_id;
};

inline DWORD WINAPI CrashReportThreadProc(LPVOID param) {
    const CrashReportRequest* request = static_cast<const CrashReportRequest*>(param);
    ReportCrash(request->exception_info, request->crashed_thread_id);
    return 0;
}

inline LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* exception_info) {
    // Si el propio handler vuelve a fallar, no reentrar: dejar que el
    // proceso termine con el codigo original.
    static volatile LONG already_handling = 0;
    if (InterlockedExchange(&already_handling, 1) != 0) return EXCEPTION_EXECUTE_HANDLER;

    const DWORD crashed_thread_id = GetCurrentThreadId();

    // Stack overflow: en este hilo casi no queda pila para el informe --
    // se genera desde un hilo nuevo con pila propia. Solo en este caso
    // (crear un hilo dentro de un crash en DllMain bloquearia por el
    // loader lock).
    if (exception_info != nullptr && exception_info->ExceptionRecord != nullptr &&
        exception_info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        static CrashReportRequest request;
        request.exception_info = exception_info;
        request.crashed_thread_id = crashed_thread_id;
        HANDLE thread = CreateThread(nullptr, 1 << 20, CrashReportThreadProc, &request, 0, nullptr);
        if (thread != nullptr) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    ReportCrash(exception_info, crashed_thread_id);
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace detail

// Public alias so runners (avanative's main) can apply the same rule to their
// own launch-failure dialog. See detail::StderrIsCapturedByParent.
inline bool StderrIsCapturedByParent() {
    return detail::StderrIsCapturedByParent();
}

// Instala el filtro de excepcion no manejada para el proceso actual.
// Llamar UNA sola vez, lo antes posible en main() -- antes de crear la VM,
// abrir ventanas, o hacer cualquier otra cosa que pueda crashear. `app_name`
// se usa para el nombre del .dmp y el titulo del MessageBox (p.ej.
// "avanative", "avapack_stub", "ava_cli") -- usa un nombre que el usuario
// pueda reconocer, no un nombre de clase interno.
inline void InstallCrashHandler(const std::string& app_name) {
    detail::CrashHandlerAppName() = app_name;
    detail::EnsureSymbolsLoaded();
    SetUnhandledExceptionFilter(detail::HandleUnhandledException);
}

}  // namespace diag
}  // namespace ava

#else  // !_WIN32

namespace ava {
namespace diag {

// Backend POSIX (sigaction + backtrace(), patron Breakpad) pendiente --
// ver Fase 1 del plan. No-op por ahora para que el codigo que llama
// InstallCrashHandler(...) compile igual en Linux/macOS.
inline void InstallCrashHandler(const std::string& /*app_name*/) {}

}  // namespace diag
}  // namespace ava

#endif  // _WIN32

#include <cstdlib>

namespace ava {
namespace diag {

inline std::string CrashDumpDirectory() {
#if defined(_WIN32)
    char temp_path[MAX_PATH] = {};
    DWORD len = GetTempPathA(MAX_PATH, temp_path);
    std::string base = (len > 0 && len < MAX_PATH) ? std::string(temp_path) : ".\\";
    return base + "avalang\\crashes\\";
#else
    const char* tmp = std::getenv("TMPDIR");
    std::string base = tmp ? std::string(tmp) : "/tmp/";
    if (!base.empty() && base.back() != '/') base += '/';
    return base + "avalang/crashes/";
#endif
}

}  // namespace diag
}  // namespace ava
