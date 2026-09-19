#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "avalang.h"
#include "vm/vm.h"
#include "build_command.h"
#include "diagnostics/crash_handler.h"
#include "diagnostics/debug_mode.h"
#include "diagnostics/error_report.h"
#include "diagnostics/vm_debug.h"
#include "dap_server.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
// Windows reconstruye argv en cada llamada a
// System.Environment.GetCommandLineArgs() via CommandLineToArgvW, asi
// que no necesita ayuda de main(). En Linux/macOS no hay forma de
// recuperar argv despues de que main() ya arranco, asi que
// Lin/MacEnvironment exponen un setter explicito que hay que llamar
// una sola vez acá con el argc/argv real del proceso -- si no se
// llama, GetCommandLineArgs() devuelve siempre una lista vacia (gap
// real encontrado en la Fase 7 de AVALANG_IMPORT_SYSTEM_PLAN.md).
#if defined(__linux__)
#include "platform/linux/LinEnvironment.h"
#elif defined(__APPLE__)
#include "platform/macos/MacEnvironment.h"
#endif
#endif

#define AVA_CLI_VERSION "0.1.0"

namespace {

// Carpeta `modules/` al lado del ejecutable (ava_cli.exe), usada como
// stdlib_path del ModuleResolver -- mismo lugar que ya usa
// AvaStudio (studio::util::ResolveDefaultModulesDir, avastudio/src/util/
// data_dir.cpp) y que vm_extern.cpp ya usa para localizar DLLs nativas
// de un `extern` (ModulesRoot). Antes ava_cli no configuraba ningun
// stdlib_path: `import mysql` (o cualquier modulo empaquetado en
// modules/<nombre>/index.ava junto a su .dll) resolvia en AvaStudio pero
// fallaba corriendo el mismo script por linea de comandos. Con esto,
// ambos frontends buscan modulos en el mismo lugar por default.
//
// Igual que StudioSettings::modules_path (avastudio/src/util/settings.h):
// esto es solo el DEFAULT. El usuario lo puede pisar explicitamente sin
// tocar C++, con --modules <dir> o la variable de entorno
// AVA_MODULES_PATH (el flag gana si estan los dos puestos).
std::string ModulesDirNextToExecutable() {
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        std::string path(exe_path);
        size_t sep = path.find_last_of("/\\");
        if (sep != std::string::npos) {
            return path.substr(0, sep) + "\\modules";
        }
    }
#else
    char exe_path[4096];
    ssize_t n = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n > 0) {
        exe_path[n] = '\0';
        std::string path(exe_path);
        size_t sep = path.find_last_of('/');
        if (sep != std::string::npos) {
            return path.substr(0, sep) + "/modules";
        }
    }
#endif
    return "modules";
}

// Extrae un flag "--nombre valor" de argv, en cualquier posicion antes del
// script (que siempre es el primer argumento que no empieza con "--").
// Devuelve "" y deja intacto argc/argv si no esta presente. Los flags
// encontrados se remueven de la lista in-place para que el resto del
// parsing (argv[1] = script, argv[2..] = argumentos del script) no se
// entere de que existieron.
std::string ExtractFlagValue(int& argc, char** argv, const char* flag_name) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == flag_name) {
            if (i + 1 >= argc) return "";
            std::string value = argv[i + 1];
            for (int j = i; j + 2 < argc; ++j) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            return value;
        }
    }
    return "";
}

// Variable de entorno como fallback de un flag: el flag explicito en la
// linea de comandos siempre gana si esta presente.
std::string EnvOrDefault(const char* var_name) {
    const char* value = std::getenv(var_name);
    return value ? std::string(value) : "";
}

std::string CompilerTag() {
#if defined(_MSC_VER)
    #if defined(_WIN64)
        return "[MSC v." + std::to_string(_MSC_VER) + " 64 bit (AMD64)]";
    #else
        return "[MSC v." + std::to_string(_MSC_VER) + " 32 bit (x86)]";
    #endif
#elif defined(__clang__)
    return "[Clang " + std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__) + "]";
#elif defined(__GNUC__)
    return "[GCC " + std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." +
           std::to_string(__GNUC_PATCHLEVEL__) + "]";
#else
    return "[unknown compiler]";
#endif
}

// Expone los argumentos extra de la linea de comandos al script como el
// global `args` (una List de strings), asi el codigo AvaLang puede leer
// `ava_cli miscript.ava par1 par2` como args = ["par1", "par2"] -- igual
// que sys.argv[1:] en Python o process.argv.slice(2) en Node. `extra`
// arranca en el primer argumento DESPUES del path del script (no incluye
// ni el nombre del ejecutable ni el path del .ava, que ya se resuelven
// aparte via argv[1]).
void SetScriptArgsGlobal(ava::VM* raw_vm, const std::vector<std::string>& script_args) {
    auto* list = new ava::ListObj();
    for (const std::string& arg : script_args) {
        list->items.push_back(ava::Value::String(arg));
    }
    ava::Value args_value;
    args_value.type = ava::ValueType::List;
    args_value.obj = list;
    raw_vm->SetGlobal("args", args_value);
}

std::string PlatformTag() {
#if defined(_WIN32)
    return "win32";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

void PrintBanner() {
    std::printf("AvaLang %s (%s, %s) %s on %s\n",
                AVA_CLI_VERSION, __DATE__, __TIME__,
                CompilerTag().c_str(), PlatformTag().c_str());
    std::printf("Type \"ava_cli --help\" for usage information.\n");
}

// Formatea un error (de compilacion o de runtime) con archivo:linea:columna
// + snippet de la fuente + caret "^", igual que cualquier compilador decente.
// Si el VM no tiene posicion asociada (line/col <= 0), cae a un mensaje
// plano con el prefijo `fallback_label` (p.ej. "compile error" o
// "runtime error").
void PrintFormattedError(AvaVM* vm, const char* script_path,
                          const std::string& err_msg,
                          const char* fallback_label) {
    int err_line = ava_last_error_line(vm);
    int err_col = ava_last_error_column(vm);
    char* err_src = ava_last_error_source(vm);
    // La columna solo existe para errores de compilacion (el parser la
    // trackea); los errores de runtime de la VM (MakeFrameError,
    // vm/vm_errors.cpp) solo saben la linea -- ahi column llega en 0.
    // Exigir columna > 0 para mostrar el formato lindo dejaba a TODO
    // error de runtime cayendo siempre al fallback plano aunque la
    // linea si fuera valida. Ahora alcanza con tener linea; el caret
    // se omite (no se dibuja "^") cuando no hay columna real.
    if (err_line > 0) {
        if (err_col > 0) {
            std::fprintf(stderr, "error at %s:%d:%d: %s\n",
                         err_src ? err_src : script_path, err_line, err_col,
                         err_msg.c_str());
        } else {
            std::fprintf(stderr, "error at %s:%d: %s\n",
                         err_src ? err_src : script_path, err_line,
                         err_msg.c_str());
        }
        // Bug pre-existente, expuesto ahora por errores que cruzan de
        // archivo (p.ej. colision de nombres entre hermanos, ver
        // compiler.cpp): esto abria siempre `script_path` (el archivo que
        // se le paso a `ava_cli` para correr) para sacar el snippet +
        // caret, sin importar en que archivo haya ocurrido el error de
        // verdad. El header de arriba ya usa `err_src` correctamente
        // (por eso decia "error at app.ava:1:1" aunque uno corriera
        // main.ava), pero el snippet abria main.ava y mostraba SU linea
        // 1 -- coincidencia que hacia parecer que el error era sobre
        // `import system` cuando en realidad estaba en otro archivo.
        // Mismo archivo que ya elegimos para el header, para el snippet.
        std::ifstream src_file(err_src ? err_src : script_path);
        if (src_file) {
            std::string src_line;
            int cur = 1;
            while (std::getline(src_file, src_line)) {
                if (cur == err_line) {
                    if (!src_line.empty() && src_line.back() == '\r') src_line.pop_back();
                    std::fprintf(stderr, "    %d | %s\n", err_line, src_line.c_str());
                    if (err_col > 0) {
                        // Mismo algoritmo que formatError() en
                        // frontend/frontend_antlr.cpp (errores de
                        // compilacion): "^" en la columna, seguido de
                        // "~" hasta el proximo separador (espacio o
                        // puntuacion), tope 20 caracteres, para
                        // subrayar el token entero en vez de un solo
                        // caracter.
                        size_t display_col = static_cast<size_t>(err_col) <= src_line.size() + 1
                            ? static_cast<size_t>(err_col) : src_line.size() + 1;
                        size_t indent = 5 + std::to_string(err_line).size();
                        std::fprintf(stderr, "%*s", static_cast<int>(indent), "");
                        for (size_t i = 1; i < display_col; ++i) {
                            if (i - 1 < src_line.size() && (src_line[i-1] == '\t' || (unsigned char)src_line[i-1] < 32))
                                std::fputc(src_line[i-1], stderr);
                            else
                                std::fputc(' ', stderr);
                        }
                        std::fputc('^', stderr);
                        if (static_cast<size_t>(err_col) <= src_line.size()) {
                            std::string token_text = src_line.substr(static_cast<size_t>(err_col) - 1);
                            size_t token_end = token_text.find_first_of(" \t\n\r.,;:!?()[]{}");
                            if (token_end == std::string::npos) token_end = token_text.size();
                            for (size_t i = 1; i < token_end && i < 20; ++i) {
                                std::fputc('~', stderr);
                            }
                        }
                        std::fputc('\n', stderr);
                    }
                    break;
                }
                ++cur;
            }
        }
    } else {
        std::fprintf(stderr, "%s: %s\n", fallback_label, err_msg.c_str());
    }

    // Fase 0 de PLAN_DEBUG_MODE_AVASTUDIO.md: ademas del texto de arriba
    // (que no cambia), una linea machine-readable para que AvaStudio la
    // pueda parsear sin depender del wording humano -- ver
    // runtime/common/diagnostics/error_report.h.
    ava::diag::ErrorInfo structured;
    structured.kind = (std::string(fallback_label) == "runtime error")
                           ? ava::diag::ErrorKind::kRuntime
                           : ava::diag::ErrorKind::kCompile;
    structured.message = err_msg;
    structured.file = err_src ? err_src : script_path;
    if (err_line > 0) structured.line = err_line;
    if (err_col > 0) structured.col = err_col;
    structured.stack = ava::diag::CaptureDebugStack(vm);
    ava::diag::PrintStackTrace(structured.stack);
    ava::diag::EmitStructuredError(structured);

    if (err_src) ava_string_free(err_src);
}

void PrintUsage(const char* argv0) {
    std::printf(
        "usage:\n"
        "  %s <script.ava>              Compile and run an AvaLang script\n"
        "  %s build [options]           Package an AvaLang project into a standalone executable\n"
        "  %s --dap-server <port> [script.ava]\n"
        "                               Start a Debug Adapter Protocol server on 127.0.0.1\n"
        "                               (port 0 picks a free port)\n"
        "  %s --help, -h                Show this help message\n"
        "  %s --version, -v             Show version information\n"
        "\n"
        "  --modules <dir>               Override the modules/ folder used to resolve\n"
        "                                 `import` (default: modules/ next to this exe).\n"
        "                                 Same effect as the AVA_MODULES_PATH env var;\n"
        "                                 the flag wins if both are set.\n"
        "\n"
        "Run '%s build --help' for packaging options.\n",
        argv0, argv0, argv0, argv0, argv0, argv0);
}

int RunScript(const std::string& script_path, const std::vector<std::string>& script_args,
              const std::string& modules_path_override, const ava::dap::ScriptHooks& hooks) {
    std::ifstream file(script_path);
    if (!file) {
        std::fprintf(stderr, "error: could not open %s\n", script_path.c_str());
        ava::diag::EmitStructuredError(ava::diag::ErrorKind::kLaunchFailure,
                                        "could not open " + script_path);
        if (hooks.on_error) hooks.on_error("could not open " + script_path);
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    AvaVM* vm = ava_vm_create();
    ava::diag::ApplyDebugMode(vm);

    {
        ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
        raw_vm->GetModuleResolver().SetStdlibPath(
            modules_path_override.empty() ? ModulesDirNextToExecutable() : modules_path_override);

        std::string script_dir = script_path;
        size_t sep = script_dir.find_last_of("/\\");
        if (sep != std::string::npos) {
            script_dir = script_dir.substr(0, sep);
            raw_vm->GetModuleResolver().AddSearchPath(script_dir);
        }

        SetScriptArgsGlobal(raw_vm, script_args);
        if (hooks.on_ready) hooks.on_ready(raw_vm);
    }

    int exit_code = 0;
    char* error = nullptr;
    AvaModule* module = ava_compile(vm, buffer.str().c_str(), script_path.c_str(), &error);
    if (!module) {
        std::string err_msg = error ? error : "unknown error";
        if (err_msg.substr(0, 9) == "error at ") {
            std::fprintf(stderr, "%s\n", err_msg.c_str());
            ava::diag::EmitStructuredError(ava::diag::ErrorKind::kCompile, err_msg);
        } else {
            PrintFormattedError(vm, script_path.c_str(), err_msg, "compile error");
        }
        if (hooks.on_error) hooks.on_error(err_msg);
        if (error) ava_string_free(error);
        exit_code = 1;
    } else {
        ava_value_t result{};
        ava_run(vm, module, &result, &error);
        if (error) {
            std::string err_msg = error;
            PrintFormattedError(vm, script_path.c_str(), err_msg, "runtime error");
            if (hooks.on_error) hooks.on_error(err_msg);
            ava_string_free(error);
            exit_code = 1;
        } else {
            ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
            while (raw_vm->HasPendingAsyncWork()) {
                raw_vm->PumpAsyncEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    if (hooks.on_closing) hooks.on_closing();
    ava_vm_destroy(vm);
    if (module) ava_module_destroy(module);
    return exit_code;
}

bool HasFlag(int argc, char** argv, const char* flag_name) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == flag_name) return true;
    }
    return false;
}

int RunDapServer(int port, const std::string& default_program, const std::string& modules_path_override) {
    ava::dap::DapServer server(
        [&modules_path_override](const ava::dap::LaunchRequest& launch, const ava::dap::ScriptHooks& hooks) {
            return RunScript(launch.program, launch.args, modules_path_override, hooks);
        },
        default_program);
    return server.Serve(port);
}

} // namespace

int main(int argc, char** argv) {
    // Fase 1 de PLAN_DEBUG_MODE_AVASTUDIO.md: instalar el filtro de
    // excepcion no manejada ANTES de cualquier otra cosa (crear la VM,
    // compilar, correr el script) -- si algo nativo crashea mas abajo, deja
    // un .dmp + una linea estructurada en vez de morir en silencio. No-op
    // fuera de Windows por ahora (ver crash_handler.h).
    ava::diag::InstallCrashHandler("ava_cli");

    // Force stdout/stderr fully unbuffered. The CRT auto-detects whether
    // a stream is attached to a real console and only line-buffers in
    // that case -- when a parent process redirects our stdout/stderr
    // into a pipe (exactly what Ava Studio's Build panel does, so it can
    // show this output live in its Output panel -- see
    // ava::platform::windows::WinProcess::ExecuteStreaming), the CRT
    // silently falls back to full buffering (several KB) instead. That
    // means every std::printf/std::cout line below (and RunTool's own
    // streamed cmake/msbuild output in build_command.cpp) would sit in
    // our own buffer and only reach the pipe once it fills up or this
    // process exits -- from the parent's side, indistinguishable from
    // "nothing happened until the very end", which defeats the whole
    // point of a real-time log. Unbuffered means every write reaches the
    // pipe (and Ava Studio's reader thread) immediately.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

#if defined(__linux__)
    // Semilla de argv real para System.Environment.GetCommandLineArgs(),
    // ANTES de que ExtractFlagValue (mas abajo) mute argc/argv quitando
    // --modules -- igual que CommandLineToArgvW en Windows, se quiere el
    // argv original tal cual lo vio el proceso, no la version ya
    // recortada que usa el resto de este main() para su propio parsing.
    ava::platform::linux_::SetCommandLineArgs(argc, argv);
#elif defined(__APPLE__)
    // Mismo esquema que __linux__ arriba, ver LinEnvironment.h/.cpp.
    ava::platform::macos_::SetCommandLineArgs(argc, argv);
#endif

    ava::diag::InitDebugRuntime(argc, argv);

    if (argc < 2) {
        PrintBanner();
        std::printf("\n");
        PrintUsage(argv[0]);
        return 1;
    }

    // Override de resolucion de modulos -- se lee y se remueve de argv
    // ANTES de mirar argv[1], para que "ava_cli.exe --modules X script.ava"
    // y "ava_cli.exe script.ava --modules X" funcionen igual. Mismo par
    // default-vacio/override-explicito que StudioSettings::modules_path en
    // AvaStudio (ver ModulesDirNextToExecutable arriba). Un solo flag --
    // antes existian --modules-path y --libraries-path por separado, pero
    // ambos terminaban seteando lo mismo (stdlib_path_ del
    // ModuleResolver), asi que quedaba como dos formas de decir lo mismo
    // sin ninguna diferencia real de comportamiento. Unificado.
    std::string modules_path_override = ExtractFlagValue(argc, argv, "--modules");
    if (modules_path_override.empty()) modules_path_override = EnvOrDefault("AVA_MODULES_PATH");

    if (HasFlag(argc, argv, "--dap-server")) {
        std::string port_text = ExtractFlagValue(argc, argv, "--dap-server");
        char* end = nullptr;
        long port = std::strtol(port_text.c_str(), &end, 10);
        if (port_text.empty() || *end != '\0' || port < 0 || port > 65535) {
            std::fprintf(stderr, "error: --dap-server requires a port between 0 and 65535\n");
            return 1;
        }
        std::string default_program = argc >= 2 ? argv[1] : "";
        return RunDapServer(static_cast<int>(port), default_program, modules_path_override);
    }

    std::string first_arg = argv[1];

    if (first_arg == "--help" || first_arg == "-h") {
        PrintBanner();
        std::printf("\n");
        PrintUsage(argv[0]);
        return 0;
    }

    if (first_arg == "--version" || first_arg == "-v") {
        std::printf("AvaLang %s (%s, %s) %s on %s\n",
                     AVA_CLI_VERSION, __DATE__, __TIME__,
                     CompilerTag().c_str(), PlatformTag().c_str());
        return 0;
    }

    if (first_arg == "build") {
        return RunBuildCommand(argc, argv);
    }

    std::vector<std::string> script_args;
    for (int i = 2; i < argc; ++i) script_args.push_back(argv[i]);
    return RunScript(argv[1], script_args, modules_path_override, ava::dap::ScriptHooks{});
}
