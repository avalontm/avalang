#include "build_command.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"

#include "avalang.h"
#include "payload_format.h" // Fase 9: footer/blob del payload apendeado a avapack_stub.exe

#if defined(_WIN32)
    #define AVACLI_EXE_SUFFIX ".exe"
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #define AVACLI_EXE_SUFFIX ""
    #include <unistd.h>
    #include <limits.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path GetSelfExecutableDir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    std::error_code ec;
    fs::path exe_path(std::string(buf, len));
    fs::path dir = exe_path.parent_path();
    return fs::exists(dir, ec) ? dir : fs::path{};
#else
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return {};
    }
    fs::path exe_path(std::string(buf, static_cast<size_t>(len)));
    return exe_path.parent_path();
#endif
}

struct BuildOptions {
    std::string project_dir;
    std::string entry_file;
    std::string out_path;
    std::string key_file;
    std::string repo_root;
    bool keep_temp = false;
    bool clean_pack = false;
    bool debug_unencrypted = false;

    bool obfuscate = false;
    bool obfuscate_strings = false;
    bool flatten_control_flow = false;

    bool zero_disk = false;

    std::string sign_pfx;
    std::string sign_password_env;
    std::string sign_timestamp_url;
    std::string target = "desktop";
    std::string toolchain_dir;            
    std::string compiler_path;
    std::optional<std::uint32_t> stack_size; 
    std::optional<std::uint32_t> bss_size; 
};

void PrintBuildUsage() {
    std::cerr <<
        "usage: ava_cli build --project <dir> --entry <file.ava> --out <exe>\n"
        "                      [--key-file <path>] [--clean] [--repo-root <dir>]\n"
        "                      [--debug] [--zero-disk]\n"
        "                      [--sign-pfx <path.pfx> [--sign-password-env <VAR>]\n"
        "                       [--sign-timestamp-url <url>]]\n"
        "\n"
        "  --project     Root folder of the AvaLang project to package.\n"
        "  --entry       Entry .ava file, relative to --project.\n"
        "  --out         Path of the final .exe. If it's a directory (existing, or\n"
        "                ending in '/' or '\\'), the binary is saved inside it using\n"
        "                a name derived from --entry (e.g. main.ava -> main.exe).\n"
        "  --key-file    Path to 32 raw bytes with the AES-256 key to use. Without\n"
        "                this flag, avapack_gen generates a random key per build.\n"
        "  --keep-temp   Kept for backwards compatibility; no longer needed --\n"
        "                build_pack/ is kept by default now (see --clean) so\n"
        "                avalang.dll isn't recompiled from scratch on every build.\n"
        "  --clean       Wipe build_pack/ before configuring, for a fully fresh\n"
        "                build (recompiles avalang.dll from scratch). Without this,\n"
        "                build_pack/ persists between runs and only what actually\n"
        "                changed gets rebuilt.\n"
        "  --repo-root   Root of the AvaLang repo. Default: current directory.\n"
        "  --debug       Packaged debug mode: the embedded content is left\n"
        "                UNENCRYPTED (so it can be diagnosed) and it is built in\n"
        "                Debug mode (with symbols) instead of Release. Do not use\n"
        "                for distribution -- it loses the Phase 3 protection. See\n"
        "                runtime/avapack/README.md.\n"
        "  --obfuscate   The --entry is compiled and serialized to .avbc bytecode\n"
        "                instead of being embedded as plain-text .ava (imports\n"
        "                remain plain-text encrypted, unchanged -- see\n"
        "                runtime/avapack/src/embedded_project.h). Also generates a\n"
        "                <out>.avmap next to the build with the SymbolMap -- keep\n"
        "                it yourself, NEVER distribute it alongside the .exe.\n"
        "  --obfuscate-strings\n"
        "                (requires --obfuscate) Also obfuscates the strings in the\n"
        "                entry's constant pool (messages, literals,\n"
        "                global/attribute names).\n"
        "  --flatten-control-flow\n"
        "                (requires --obfuscate) Also flattens the entry's control\n"
        "                flow (single dispatcher instead of direct JMP/TEST).\n"
        "  --zero-disk   The packaged project is resolved against an in-memory\n"
        "                virtual filesystem instead of being materialized in a temp\n"
        "                dir -- no .ava/.avbc from the project touches disk, not\n"
        "                even for milliseconds. Replaces the previous scheme\n"
        "                (compiles main_zerodisk.cpp instead of main.cpp, see\n"
        "                runtime/avapack/README.md). Combinable with --obfuscate\n"
        "                and --debug.\n"
        "  --sign-pfx    (optional, requires signtool in PATH) Signs the final .exe\n"
        "                with the given .pfx certificate. Without this flag, the\n"
        "                .exe is not signed.\n"
        "  --sign-password-env\n"
        "                Name of an environment variable that holds the .pfx\n"
        "                password (NOT the password itself -- so it never ends up\n"
        "                in shell history or the process list). If the .pfx has no\n"
        "                password, omit this flag.\n"
        "  --sign-timestamp-url\n"
        "                URL of an RFC 3161 timestamp server (e.g.\n"
        "                http://timestamp.digicert.com). Recommended: without a\n"
        "                timestamp, the signature becomes invalid once the\n"
        "                certificate expires, even if the binary hasn't changed.\n"
        "\n"
        "  --compiler-path <dir>\n"
        "                Folder containing the real compiler/toolchain to use --\n"
        "                same flag for BOTH targets, just point it at a path:\n"
        "                  * --target desktop: prepended to PATH before invoking\n"
        "                    cmake, so it can pick up a portable cmake.exe/ninja.exe\n"
        "                    or a host gcc/g++/cl.exe placed there instead of\n"
        "                    whatever is already on the system PATH.\n"
        "                  * --target barekernel: used as the i686-elf toolchain\n"
        "                    root (same thing --toolchain-dir does) if\n"
        "                    --toolchain-dir isn't also given -- searched\n"
        "                    recursively for i686-elf-gcc/g++/ld/objcopy/nm.\n"
        "                --toolchain-dir still works on its own for barekernel if\n"
        "                you'd rather keep them separate; --compiler-path is just\n"
        "                the one-flag-for-both shortcut.\n"
        "  --target <desktop|barekernel>\n"
        "                Default 'desktop' (everything above). 'barekernel'\n"
        "                switches to Fase B2 of plan_avapack_barekernel.md: packages\n"
        "                --entry as an AppHeader .exe for litekernel instead of a\n"
        "                Windows PE. Ignores --obfuscate/--zero-disk/--sign-*/\n"
        "                --key-file (none apply to that target yet). Requires\n"
        "                --toolchain-dir.\n"
        "  --toolchain-dir <dir>\n"
        "                (--target barekernel only) folder containing the i686-elf\n"
        "                cross-compiler (i686-elf-gcc/g++/ld/objcopy/nm), same as\n"
        "                scripts/build_barekernel.bat's TOOLCHAIN_DIR argument.\n"
        "  --stack-size <N>\n"
        "                (--target barekernel only) stack size in bytes for the\n"
        "                AppHeader. Default: 65536 (AppBuilder's real default, see\n"
        "                apphdr_writer.h).\n"
        "  --bss-size <N>\n"
        "                (--target barekernel only) manual override for .bss size in\n"
        "                bytes in the AppHeader. Normally NOT needed: by default this\n"
        "                is computed automatically from the linked app.elf's section\n"
        "                table (sum of SHT_NOBITS+SHF_ALLOC sections, see\n"
        "                elf32_bss.h), so it always matches what the binary actually\n"
        "                needs. Only pass this for edge cases where the auto-computed\n"
        "                value is wrong for your program.\n";
}

bool ParseBuildArgs(int argc, char** argv, BuildOptions& opts, std::string& error) {
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        auto next_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                error = std::string("missing value for ") + flag;
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--project") {
            const char* v = next_value("--project"); if (!v) return false;
            opts.project_dir = v;
        } else if (arg == "--entry") {
            const char* v = next_value("--entry"); if (!v) return false;
            opts.entry_file = v;
        } else if (arg == "--out") {
            const char* v = next_value("--out"); if (!v) return false;
            opts.out_path = v;
        } else if (arg == "--key-file") {
            const char* v = next_value("--key-file"); if (!v) return false;
            opts.key_file = v;
        } else if (arg == "--repo-root") {
            const char* v = next_value("--repo-root"); if (!v) return false;
            opts.repo_root = v;
        } else if (arg == "--keep-temp") {
            opts.keep_temp = true;
        } else if (arg == "--clean") {
            opts.clean_pack = true;
        } else if (arg == "--debug") {
            opts.debug_unencrypted = true;
        } else if (arg == "--obfuscate") {
            opts.obfuscate = true;
        } else if (arg == "--obfuscate-strings") {
            opts.obfuscate_strings = true;
        } else if (arg == "--flatten-control-flow") {
            opts.flatten_control_flow = true;
        } else if (arg == "--zero-disk") {
            opts.zero_disk = true;
        } else if (arg == "--sign-pfx") {
            const char* v = next_value("--sign-pfx"); if (!v) return false;
            opts.sign_pfx = v;
        } else if (arg == "--sign-password-env") {
            const char* v = next_value("--sign-password-env"); if (!v) return false;
            opts.sign_password_env = v;
        } else if (arg == "--sign-timestamp-url") {
            const char* v = next_value("--sign-timestamp-url"); if (!v) return false;
            opts.sign_timestamp_url = v;
        } else if (arg == "--target") {
            const char* v = next_value("--target"); if (!v) return false;
            opts.target = v;
        } else if (arg == "--toolchain-dir") {
            const char* v = next_value("--toolchain-dir"); if (!v) return false;
            opts.toolchain_dir = v;
        } else if (arg == "--compiler-path") {
            const char* v = next_value("--compiler-path"); if (!v) return false;
            opts.compiler_path = v;
        } else if (arg == "--stack-size") {
            const char* v = next_value("--stack-size"); if (!v) return false;
            char* end = nullptr;
            unsigned long parsed = std::strtoul(v, &end, 0);
            if (end == v || *end != '\0') { error = "invalid --stack-size"; return false; }
            opts.stack_size = static_cast<std::uint32_t>(parsed);
        } else if (arg == "--bss-size") {
            const char* v = next_value("--bss-size"); if (!v) return false;
            char* end = nullptr;
            unsigned long parsed = std::strtoul(v, &end, 0);
            if (end == v || *end != '\0') { error = "invalid --bss-size"; return false; }
            opts.bss_size = static_cast<std::uint32_t>(parsed);
        } else if (arg == "--help" || arg == "-h") {
            PrintBuildUsage();
            std::exit(0);
        } else {
            error = "unknown flag: " + arg;
            return false;
        }
        if (!error.empty()) return false;
    }

    if (opts.project_dir.empty()) { error = "missing --project"; return false; }
    if (opts.entry_file.empty()) { error = "missing --entry"; return false; }
    if (opts.out_path.empty()) { error = "missing --out"; return false; }
    if (opts.target != "desktop" && opts.target != "barekernel") {
        error = "--target must be 'desktop' or 'barekernel' (got '" + opts.target + "')";
        return false;
    }
    if (opts.target == "barekernel" && opts.toolchain_dir.empty() && opts.compiler_path.empty()) {
        error = "--target barekernel requires --toolchain-dir or --compiler-path "
                "<folder with i686-elf-gcc/g++/ld/objcopy/nm>";
        return false;
    }
    if (opts.target == "barekernel" && opts.toolchain_dir.empty()) {

        opts.toolchain_dir = opts.compiler_path;
    }
    if (!opts.sign_pfx.empty() && opts.sign_pfx.substr(0, 2) == "--") {
        error = "--sign-pfx seems to be missing its value (got '" + opts.sign_pfx + "')";
        return false;
    }
    if ((opts.obfuscate_strings || opts.flatten_control_flow) && !opts.obfuscate) {
        error = "--obfuscate-strings/--flatten-control-flow require --obfuscate";
        return false;
    }
    return true;
}

bool RunTool(ava::platform::IProcess& process, const std::string& command,
             const std::vector<std::string>& args, const std::string& step_label) {

    if (auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process)) {
        int exit_code = -1;
        const bool launched = streaming->ExecuteStreaming(
            command, args,
            [](const std::string& chunk) { std::cout << chunk; },
            exit_code);
        if (!launched) {
            std::cerr << "error: could not run '" << command << "' -- "
                      << "is it installed and in PATH?\n";
            std::cerr << "       (step: " << step_label << ")\n";
            return false;
        }
        if (exit_code != 0) {
            std::cerr << "error: " << step_label << " failed (exit code " << exit_code << ")\n";
            return false;
        }
        return true;
    }

    ava::platform::ProcessResult result;
    bool launched = process.Execute(command, args, result);
    if (!launched) {
        std::cerr << "error: could not run '" << command << "' -- "
                  << "is it installed and in PATH?\n";
        std::cerr << "       (step: " << step_label << ")\n";
        return false;
    }
    if (result.exit_code != 0) {
        std::cerr << "error: " << step_label << " failed (exit code " << result.exit_code << ")\n";
        if (!result.stdout_output.empty()) std::cerr << result.stdout_output << "\n";
        if (!result.stderr_output.empty()) std::cerr << result.stderr_output << "\n";
        return false;
    }
    if (!result.stdout_output.empty()) std::cout << result.stdout_output;
    return true;
}

std::optional<fs::path> FindBuiltBinary(const fs::path& build_dir, const std::string& target_name,
                                         const std::string& config) {
    fs::path multi_config = build_dir / "runtime" / "avalang" / config /
                             (target_name + AVACLI_EXE_SUFFIX);
    if (fs::exists(multi_config)) return multi_config;

    fs::path single_config = build_dir / "runtime" / "avalang" / (target_name + AVACLI_EXE_SUFFIX);
    if (fs::exists(single_config)) return single_config;

    return std::nullopt;
}

void CopyRuntimeDllIfPresent(const fs::path& built_binary_dir, const fs::path& out_dir,
                              const std::string& dll_name) {
    fs::path src = built_binary_dir / dll_name;
    if (!fs::exists(src)) return;
    std::error_code ec;
    fs::copy_file(src, out_dir / dll_name, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "warning: could not copy " << dll_name << " next to the final .exe ("
                  << ec.message() << ")\n";
    }
}

void CopyAvaUiDllIfAvailable(const fs::path& built_binary_dir, const fs::path& out_dir,
                              const fs::path& repo_root, const std::string& config) {
    const std::string dll_name = "avalang_ui.dll";

    fs::path src = built_binary_dir / dll_name;
    if (!fs::exists(src)) {
        fs::path cli_multi = repo_root / "build_cli" / "runtime" / "avaui" / config / dll_name;
        fs::path cli_single = repo_root / "build_cli" / "runtime" / "avaui" / dll_name;
        if (fs::exists(cli_multi)) {
            src = cli_multi;
        } else if (fs::exists(cli_single)) {
            src = cli_single;
        } else {
            return;
        }
    }

    std::error_code ec;
    fs::copy_file(src, out_dir / dll_name, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "warning: could not copy " << dll_name << " next to the final .exe ("
                  << ec.message() << ")\n";
    }
}

std::vector<std::string> FindPatternNames(const std::string& content, const std::regex& re) {
    std::vector<std::string> out;
    auto begin = std::sregex_iterator(content.begin(), content.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) out.push_back((*it)[1].str());
    return out;
}

bool ReadFileToString(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::unordered_set<std::string> CollectExternLibraryNames(const fs::path& project_dir,
                                                            const fs::path& libraries_dir) {
    static const std::regex import_re(
        R"(\bimport\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*))");
    static const std::regex extern_re(R"re(\bextern\s+"([A-Za-z0-9_]+)")re");

    std::unordered_set<std::string> extern_names;
    std::unordered_set<std::string> visited_modules;
    std::vector<std::string> pending_modules;

    auto scan_file = [&](const fs::path& p) {
        std::string content;
        if (!ReadFileToString(p, content)) return;
        for (auto& n : FindPatternNames(content, extern_re)) extern_names.insert(n);
        for (auto& full : FindPatternNames(content, import_re)) {
            auto dot = full.find('.');
            std::string top = dot == std::string::npos ? full : full.substr(0, dot);
            if (visited_modules.insert(top).second) pending_modules.push_back(top);
        }
    };

    std::error_code ec;
    auto scan_tree = [&](const fs::path& root) {
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return;
        for (auto it = fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator() && !ec; it.increment(ec)) {
            std::error_code fe;
            if (!it->is_regular_file(fe)) continue;
            std::string ext = it->path().extension().string();
            if (ext == ".ava" || ext == ".avaui") scan_file(it->path());
        }
    };

    scan_tree(project_dir);

    while (!pending_modules.empty()) {
        std::string name = pending_modules.back();
        pending_modules.pop_back();
        scan_tree(libraries_dir / name);
    }

    return extern_names;
}

std::optional<fs::path> FindDllForExternName(const fs::path& libraries_dir, const std::string& name) {
    std::error_code ec;
    if (!fs::exists(libraries_dir, ec) || !fs::is_directory(libraries_dir, ec)) return std::nullopt;

#if defined(_WIN32)
    std::vector<std::string> wanted_names = { name + ".dll" };
#else
    std::vector<std::string> wanted_names = { "lib" + name + ".so", name + ".so" };
#endif

    std::vector<std::string> wanted_lower;
    for (auto& w : wanted_names) {
        std::string wl = w;
        std::transform(wl.begin(), wl.end(), wl.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        wanted_lower.push_back(wl);
    }

    for (auto it = fs::recursive_directory_iterator(
             libraries_dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && !ec; it.increment(ec)) {
        std::error_code fe;
        if (!it->is_regular_file(fe)) continue;
        std::string fname = it->path().filename().string();
        std::string fname_lower = fname;
        std::transform(fname_lower.begin(), fname_lower.end(), fname_lower.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (auto& w : wanted_lower) {
            if (fname_lower == w) return it->path();
        }
    }
    return std::nullopt;
}

void CopyExternNativeLibraries(const fs::path& project_dir_abs, const fs::path& libraries_dir,
                                const fs::path& out_dir) {
    auto extern_names = CollectExternLibraryNames(project_dir_abs, libraries_dir);
    if (extern_names.empty()) return;

    std::unordered_map<std::string, std::string> used_filenames;
    std::error_code ec;
    for (auto& name : extern_names) {
        auto dll = FindDllForExternName(libraries_dir, name);
        if (!dll) {
            std::cout << "[WARN] extern \"" << name << "\" has no matching DLL under "
                      << libraries_dir.string() << " -- assuming a system library.\n";
            continue;
        }

        std::string filename = dll->filename().string();
        fs::path dest = out_dir / filename;

        auto existing = used_filenames.find(filename);
        if (existing != used_filenames.end() && existing->second != dll->string()) {
            dest = out_dir / "modules" / name / filename;
            fs::create_directories(dest.parent_path(), ec);
        }
        used_filenames[filename] = dll->string();

        fs::copy_file(*dll, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "warning: could not copy " << dll->string() << " to " << dest.string()
                      << " (" << ec.message() << ")\n";
        } else {
            std::cout << "[info] copied " << filename << " -> " << dest.string() << "\n";
        }
    }
}

bool SignBinary(ava::platform::IProcess& process, const fs::path& exe_path,
                 const std::string& pfx_path, const std::string& password_env,
                 const std::string& timestamp_url) {
    std::vector<std::string> args = {
        "sign",
        "/f", pfx_path,
        "/fd", "sha256",
    };
    if (!password_env.empty()) {
        const char* password = std::getenv(password_env.c_str());
        if (!password) {
            std::cerr << "error: --sign-password-env=" << password_env
                      << " but that environment variable is not set\n";
            return false;
        }
        args.push_back("/p");
        args.push_back(password);
    }
    if (!timestamp_url.empty()) {
        args.push_back("/tr");
        args.push_back(timestamp_url);
        args.push_back("/td");
        args.push_back("sha256");
    }
    args.push_back(exe_path.string());

    std::cout << "ava_cli build: signing " << exe_path.string() << " with signtool ...\n";
    return RunTool(process, "signtool", args, "signtool sign");
}

// --- Fase 9: camino rapido sin repo/CMake -- ver runtime/avapack/README.md ---
//
// Antes de esto, `ava_cli build --target desktop` SIEMPRE recompilaba
// avapack (main.cpp + embedded_project.cpp generado) desde fuente via CMake
// en build_pack/ -- eso significa que, aunque avalang.dll/avalang_ui.dll
// estuvieran prebuilt junto a ava_cli.exe (AVA_PACK_USE_PREBUILT_AVALANG),
// `ava_cli build` seguia necesitando el repo COMPLETO al lado (CMakeLists.txt
// de runtime/avapack/, tiny-aes-c, checksum/, un toolchain C++) para poder
// empacar CUALQUIER proyecto -- ver la conversacion que motivo esta fase.
//
// Este camino evita todo eso: si avapack_stub.exe (un binario generico, sin
// ningun proyecto embebido -- ver src/stub_main.cpp) y avapack_gen.exe estan
// prebuilt junto a ava_cli.exe (ver scripts/build_pack_tools.bat), alcanza
// con correr avapack_gen.exe --payload-out para generar el blob cifrado y
// apendearlo (mas un footer de tamaño fijo, ver payload_format.h) al final
// de una copia de avapack_stub.exe. Sin cmake, sin compilar nada, sin
// necesitar el repo para nada mas que --extra-modules-dir (libraries/, si
// existe -- opcional, no bloquea el camino rapido si no esta).
bool FindPrebuiltPackTools(const fs::path& dir, fs::path& out_stub_exe, fs::path& out_gen_exe) {
    if (dir.empty()) return false;
    fs::path stub = dir / ("avapack_stub" + std::string(AVACLI_EXE_SUFFIX));
    fs::path gen = dir / ("avapack_gen" + std::string(AVACLI_EXE_SUFFIX));
    std::error_code ec;
    if (!fs::exists(stub, ec) || !fs::exists(gen, ec)) return false;
    if (!fs::exists(dir / "avalang.dll", ec) || !fs::exists(dir / "avalang_ui.dll", ec)) {
        // avapack_stub.exe necesita las mismas DLL que ava_cli para arrancar
        // -- sin ellas el .exe empacado tampoco va a poder correr, asi que
        // no vale la pena intentar este camino.
        return false;
    }
    out_stub_exe = stub;
    out_gen_exe = gen;
    return true;
}

// Devuelve true si el build se completo por este camino (exito o error ya
// reportado por stderr -- el llamador no debe reintentar con CMake en
// ninguno de los dos casos). Devuelve false solo cuando este camino ni
// siquiera aplica (target != desktop, --zero-disk, o no hay herramientas
// prebuilt) -- ahi si el llamador debe caer al flujo con CMake de siempre.
bool TryFastPackWithPrebuiltStub(ava::platform::IProcess& process, const BuildOptions& opts,
                                  const fs::path& project_dir_abs, const fs::path& key_file_abs,
                                  const fs::path& sign_pfx_abs, const fs::path& out_path_abs,
                                  const fs::path& repo_root, bool& ok) {
    if (opts.target != "desktop") return false;
    if (opts.zero_disk) {
        // main_zerodisk.cpp (Fase 7) no esta wireado al stub todavia -- ver
        // README.md, seccion Fase 9, "Pendiente". Cae al flujo con CMake.
        return false;
    }

    fs::path prebuilt_dir = GetSelfExecutableDir();
    fs::path stub_exe, gen_exe;
    if (!FindPrebuiltPackTools(prebuilt_dir, stub_exe, gen_exe)) return false;

    std::cout << "ava_cli build: usando herramientas prebuilt (" << stub_exe.string() << " + "
              << gen_exe.string() << ") -- sin CMake ni repo (Fase 9).\n";

    std::error_code ec;
    fs::path tmp_payload = fs::temp_directory_path(ec) /
        fs::path("avapack_payload_" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  ".bin");

    std::vector<std::string> gen_args = {
        "--project", project_dir_abs.string(),
        "--entry", opts.entry_file,
        "--payload-out", tmp_payload.string(),
    };
    if (!key_file_abs.empty()) {
        gen_args.push_back("--key-file");
        gen_args.push_back(key_file_abs.string());
    }
    if (opts.debug_unencrypted) gen_args.push_back("--debug");
    if (opts.obfuscate) {
        gen_args.push_back("--obfuscate");
        if (opts.obfuscate_strings) gen_args.push_back("--obfuscate-strings");
        if (opts.flatten_control_flow) gen_args.push_back("--flatten-control-flow");
    }
    fs::path libraries_dir = repo_root / "libraries";
    if (fs::exists(libraries_dir, ec) && fs::is_directory(libraries_dir, ec)) {
        gen_args.push_back("--extra-modules-dir");
        gen_args.push_back(libraries_dir.string());
    }

    if (!RunTool(process, gen_exe.string(), gen_args, "avapack_gen (payload, Fase 9)")) {
        fs::remove(tmp_payload, ec);
        ok = false;
        return true; // el camino aplicaba -- no reintentar con CMake, el error ya es del proyecto
    }

    std::ifstream stub_in(stub_exe, std::ios::binary);
    std::ifstream payload_in(tmp_payload, std::ios::binary);
    if (!stub_in || !payload_in) {
        std::cerr << "error: no se pudo leer avapack_stub.exe o el payload generado\n";
        fs::remove(tmp_payload, ec);
        ok = false;
        return true;
    }

    std::uint64_t stub_size = static_cast<std::uint64_t>(fs::file_size(stub_exe, ec));
    std::uint64_t payload_size = static_cast<std::uint64_t>(fs::file_size(tmp_payload, ec));

    {
        std::ofstream out_file(out_path_abs, std::ios::binary | std::ios::trunc);
        if (!out_file) {
            std::cerr << "error: could not write " << out_path_abs.string() << "\n";
            fs::remove(tmp_payload, ec);
            ok = false;
            return true;
        }
        out_file << stub_in.rdbuf();
        out_file << payload_in.rdbuf();

        avapack::PayloadFooter footer;
        std::memcpy(footer.magic, avapack::kFooterMagic, 8);
        footer.version = avapack::kFooterVersion;
        footer.flags = 0;
        footer.blob_offset = stub_size;
        footer.blob_size = payload_size;
        std::vector<unsigned char> footer_bytes = avapack::EncodeFooter(footer);
        out_file.write(reinterpret_cast<const char*>(footer_bytes.data()),
                        static_cast<std::streamsize>(footer_bytes.size()));
        if (!out_file) {
            std::cerr << "error: fallo al escribir " << out_path_abs.string() << "\n";
            fs::remove(tmp_payload, ec);
            ok = false;
            return true;
        }
    }

    fs::remove(tmp_payload, ec);

    fs::path out_dir = out_path_abs.parent_path();
    CopyRuntimeDllIfPresent(prebuilt_dir, out_dir, "avalang.dll");
    CopyRuntimeDllIfPresent(prebuilt_dir, out_dir, "avalang_ui.dll");
    if (fs::exists(libraries_dir, ec) && fs::is_directory(libraries_dir, ec)) {
        CopyExternNativeLibraries(project_dir_abs, libraries_dir, out_dir);
    }

    if (opts.obfuscate) {
        fs::path avmap_src = tmp_payload;
        avmap_src.replace_extension(".avmap");
        if (fs::exists(avmap_src, ec)) {
            fs::path avmap_dst = out_path_abs;
            avmap_dst.replace_extension(".avmap");
            fs::copy_file(avmap_src, avmap_dst, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                std::cout << "[info] SymbolMap saved to " << avmap_dst.string()
                          << " -- do NOT distribute it alongside the .exe.\n";
            }
            fs::remove(avmap_src, ec);
        }
    }

    if (!sign_pfx_abs.empty()) {
        if (!SignBinary(process, out_path_abs, sign_pfx_abs.string(), opts.sign_password_env,
                         opts.sign_timestamp_url)) {
            std::cerr << "warning: code signing failed -- " << out_path_abs.string()
                      << " remains unsigned (the rest of the build succeeded).\n";
        }
    }

    std::cout << "ava_cli build: done -> " << out_path_abs.string()
              << " (Fase 9 -- sin repo/CMake)\n";
    ok = true;
    return true;
}

std::optional<fs::path> FindI686ElfTool(const fs::path& toolchain_dir, const std::string& basename) {
    std::error_code ec;
    if (!fs::exists(toolchain_dir, ec) || !fs::is_directory(toolchain_dir, ec)) {
        return std::nullopt;
    }
    std::string wanted = basename;
#if defined(_WIN32)
    wanted += ".exe";
#endif
    for (auto it = fs::recursive_directory_iterator(
             toolchain_dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && !ec; it.increment(ec)) {
        std::error_code fe;
        if (!it->is_regular_file(fe)) continue;
        if (it->path().filename().string() == wanted) return it->path();
    }
    return std::nullopt;
}

std::optional<fs::path> FindHostToolBinary(const fs::path& build_dir, const std::string& target_name,
                                            const std::string& config) {
    fs::path multi_config = build_dir / "runtime" / "avalang" / config /
                             (target_name + AVACLI_EXE_SUFFIX);
    if (fs::exists(multi_config)) return multi_config;
    fs::path single_config = build_dir / "runtime" / "avalang" / (target_name + AVACLI_EXE_SUFFIX);
    if (fs::exists(single_config)) return single_config;
    return std::nullopt;
}

bool CompileEntryToAvb(const std::string& entry_source, const std::string& entry_rel_name,
                        const fs::path& out_avb_path, std::string& error_out) {
    AvaVM* vm = ava_vm_create();
    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, entry_source.c_str(), entry_rel_name.c_str(), &compile_error);
    if (!module) {
        error_out = compile_error ? compile_error : "unknown compile error";
        if (compile_error) ava_string_free(compile_error);
        ava_vm_destroy(vm);
        return false;
    }

    size_t bc_len = 0;
    char* symbol_map = nullptr;
    uint8_t* bc = ava_module_serialize(module, /*options=*/nullptr, &bc_len, &symbol_map);
    // Orden invertido: ver comentario en ava_barekernel_runner.cpp sobre
    // el use-after-free de teardown cuando module_destroy libera antes
    // que el VM suelte sus propias referencias compartidas.
    ava_vm_destroy(vm);
    ava_module_destroy(module);
    if (symbol_map) ava_string_free(symbol_map); 
    if (!bc) {
        error_out = "ava_module_serialize devolvio NULL";
        return false;
    }

    std::error_code ec;
    fs::create_directories(out_avb_path.parent_path(), ec);
    std::ofstream out(out_avb_path, std::ios::binary);
    bool wrote_ok = static_cast<bool>(out);
    if (wrote_ok) {
        out.write(reinterpret_cast<const char*>(bc), static_cast<std::streamsize>(bc_len));
        wrote_ok = out.good();
    }
    ava_string_free(reinterpret_cast<char*>(bc));

    if (!wrote_ok) {
        error_out = "no se pudo escribir " + out_avb_path.string();
        return false;
    }
    return true;
}

int RunBuildBarekernelCommand(ava::platform::IProcess& process, const BuildOptions& opts,
                               const fs::path& repo_root, const fs::path& project_dir_abs,
                               const fs::path& out_path_abs) {
    std::error_code ec;

    fs::path entry_path = project_dir_abs / opts.entry_file;
    if (!fs::exists(entry_path, ec)) {
        std::cerr << "error: --entry no existe: " << entry_path.string() << "\n";
        return 1;
    }
    std::string entry_source;
    if (!ReadFileToString(entry_path, entry_source)) {
        std::cerr << "error: no se pudo leer --entry: " << entry_path.string() << "\n";
        return 1;
    }

    fs::path work_dir = repo_root / "build_pack_barekernel";
    fs::path avb_path = work_dir / "entry.avb";
    fs::path embedded_cpp_path = work_dir / "embedded_avb.cpp";
    fs::path elf_path = work_dir / "app.elf";
    fs::path bin_path = work_dir / "app.bin";

    std::cout << "ava_cli build --target barekernel: compilando " << opts.entry_file
              << " a bytecode .avb ...\n";
    std::string compile_error;
    if (!CompileEntryToAvb(entry_source, opts.entry_file, avb_path, compile_error)) {
        std::cerr << "error: el entry no compilo/serializo: " << compile_error << "\n";
        return 1;
    }
    fs::path host_build_dir = repo_root / "build_pack";
    const std::string host_config = "Release";
    std::cout << "ava_cli build --target barekernel: configurando herramientas de host ("
              << host_build_dir.string() << ") ...\n";
    std::vector<std::string> host_configure_args = {
        "-S", repo_root.string(),
        "-B", host_build_dir.string(),
        "-DAVA_BUILD_PACK=ON",
        "-DAVA_BUILD_CLI=OFF",
        "-DAVA_BUILD_STUDIO=OFF",
        "-DAVA_BUILD_AVAHOST=OFF",
        "-DCMAKE_BUILD_TYPE=" + host_config,
    };

    fs::path prebuilt_dir = GetSelfExecutableDir();
    bool have_prebuilt = !prebuilt_dir.empty()
        && fs::exists(prebuilt_dir / "avalang.dll")
        && fs::exists(prebuilt_dir / "avalang.lib")
        && fs::exists(prebuilt_dir / "avalang_ui.dll")
        && fs::exists(prebuilt_dir / "avalang_ui.lib");
    if (have_prebuilt) {
        host_configure_args.push_back("-DAVA_PACK_USE_PREBUILT_AVALANG=ON");
        host_configure_args.push_back("-DAVA_PREBUILT_AVALANG_DIR=" + prebuilt_dir.string());
    }
    if (!RunTool(process, "cmake", host_configure_args, "cmake (configure herramientas de host)")) {
        return 1;
    }
    std::vector<std::string> host_build_args = {
        "--build", host_build_dir.string(),
        "--target", "avapack_barekernel_gen",
        "--target", "ava_apphdr_writer",
        "--config", host_config,
        "--parallel",
    };
    std::cout << "ava_cli build --target barekernel: compilando herramientas de host ...\n";
    if (!RunTool(process, "cmake", host_build_args, "cmake --build (herramientas de host)")) {
        return 1;
    }

    auto gen_bin = FindHostToolBinary(host_build_dir, "avapack_barekernel_gen", host_config);
    auto apphdr_bin = FindHostToolBinary(host_build_dir, "ava_apphdr_writer", host_config);
    if (!gen_bin || !apphdr_bin) {
        std::cerr << "error: avapack_barekernel_gen/ava_apphdr_writer no aparecieron bajo "
                  << host_build_dir.string() << "/runtime/avalang/ tras el build -- revisar el log.\n";
        return 1;
    }

    std::string entry_name = fs::path(opts.entry_file).stem().string();
    std::vector<std::string> gen_args = {
        "--avb", avb_path.string(),
        "--out", embedded_cpp_path.string(),
        "--entry-name", entry_name,
    };
    std::cout << "ava_cli build --target barekernel: generando embedded_avb.cpp ...\n";
    if (!RunTool(process, gen_bin->string(), gen_args, "avapack_barekernel_gen")) {
        return 1;
    }

    fs::path cross_build_dir = repo_root / "build_barekernel_pack";
    std::cout << "ava_cli build --target barekernel: configurando build cruzado ("
              << cross_build_dir.string() << ", toolchain " << opts.toolchain_dir << ") ...\n";
    std::vector<std::string> cross_configure_args = {
        "-S", repo_root.string(),
        "-B", cross_build_dir.string(),
        "-G", "Ninja", 
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=" + (repo_root / "cmake" / "toolchain-i686-elf.cmake").string(),
        "-DAVA_I686_ELF_TOOLCHAIN_DIR=" + opts.toolchain_dir,
        "-DAVA_TARGET_BAREKERNEL=ON",
        "-DAVA_BUILD_CLI=OFF",
        "-DAVA_BUILD_UI=OFF",
        "-DAVA_BUILD_STUDIO=OFF",
        "-DAVA_BUILD_AVAHOST=OFF",
        "-DAVA_BUILD_PACK=OFF",
        "-DAVAPACK_BAREKERNEL_EMBEDDED_AVB_CPP=" + embedded_cpp_path.string(),
    };
    if (!RunTool(process, "cmake", cross_configure_args, "cmake (configure cruzado)")) {
        return 1;
    }
    std::vector<std::string> cross_build_args = {
        "--build", cross_build_dir.string(),
        "--target", "avalang",
        "--target", "avalang_barekernel_so",
        "--target", "avapack_barekernel_app",
        "--parallel",
    };
    std::cout << "ava_cli build --target barekernel: compilando (cruzado, i686-elf) ...\n";
    if (!RunTool(process, "cmake", cross_build_args, "cmake --build (cruzado)")) {
        std::cerr << "error: el build cruzado fallo -- si es una pared de \"undefined reference\", "
                     "es CKM_CAP_LIBSTDCPP=0 (ver docs/kernel/binding-status.md), no necesariamente "
                     "un bug de este comando.\n";
        return 1;
    }

    fs::path app_lib = cross_build_dir / "avapack_barekernel_app" / "libavapack_barekernel_app.a";
    if (!fs::exists(app_lib, ec)) {
        std::cerr << "error: no se encontro " << app_lib.string() << " tras el build cruzado.\n";
        return 1;
    }

    auto ld_bin = FindI686ElfTool(opts.toolchain_dir, "i686-elf-ld");
    auto objcopy_bin = FindI686ElfTool(opts.toolchain_dir, "i686-elf-objcopy");
    auto nm_bin = FindI686ElfTool(opts.toolchain_dir, "i686-elf-nm");
    if (!ld_bin || !objcopy_bin || !nm_bin) {
        std::cerr << "error: no se encontraron i686-elf-ld/objcopy/nm bajo --toolchain-dir '"
                  << opts.toolchain_dir << "' -- el mismo toolchain que necesita "
                     "cmake/toolchain-i686-elf.cmake (i686-elf-gcc/g++) tiene que traer estos "
                     "tambien.\n";
        return 1;
    }

    fs::path app_ld = repo_root / "runtime" / "avapack" / "src" / "barekernel" / "app.ld";
    std::vector<std::string> ld_args = {
        "-T", app_ld.string(),
        "-o", elf_path.string(),
        // app_lib es el UNICO input de este link, asi que _start (y todo
        // lo demas) solo va a entrar si ld decide extraer el miembro del
        // .a -- normalmente ENTRY(_start) fuerza eso, pero algunas
        // versiones de ld no lo garantizan cuando no hay ningun otro .o
        // con una referencia pendiente. --whole-archive elimina la
        // ambiguedad: mete TODOS los objetos del .a, sin depender de
        // resolucion de simbolos.
        "--whole-archive",
        app_lib.string(),
        "--no-whole-archive",
    };
    std::vector<std::string> objcopy_args = { "-O", "binary", elf_path.string(), bin_path.string() };

    // Confirmado en la practica (Windows): el .a que acaba de escribir
    // ninja/ar puede no estar "asentado" todavia (AV escaneandolo, handle
    // sin liberar del todo) cuando ld lo abre inmediatamente despues --
    // el mismo comando, corrido a mano unos segundos mas tarde contra los
    // mismos archivos, encuentra _start sin problema. En vez de fallar a
    // la primera, reintentamos el link completo unas pocas veces con una
    // espera corta antes de darlo por perdido.
    constexpr int kMaxLinkAttempts = 3;
    constexpr auto kLinkRetryDelay = std::chrono::milliseconds(600);

    std::uint32_t start_address = 0;
    bool found_start = false;
    ava::platform::ProcessResult nm_result;

    for (int attempt = 1; attempt <= kMaxLinkAttempts; ++attempt) {
        std::cout << "ava_cli build --target barekernel: linkeando (" << app_ld.string() << ") "
                  << "intento " << attempt << "/" << kMaxLinkAttempts << " ...\n";
        if (!RunTool(process, ld_bin->string(), ld_args, "i686-elf-ld")) {
            return 1;
        }
        if (!RunTool(process, objcopy_bin->string(), objcopy_args, "i686-elf-objcopy")) {
            return 1;
        }

        nm_result = ava::platform::ProcessResult{};
        if (!process.Execute(nm_bin->string(), { elf_path.string() }, nm_result) ||
            nm_result.exit_code != 0) {
            std::cerr << "error: no se pudo correr i686-elf-nm sobre " << elf_path.string() << "\n";
            return 1;
        }

        found_start = false;
        std::istringstream nm_lines(nm_result.stdout_output);
        std::string line;
        static const std::regex start_line_re(R"(^([0-9a-fA-F]+)\s+\S\s+_start$)");
        while (std::getline(nm_lines, line)) {
            // i686-elf-nm.exe en Windows escribe \r\n; std::getline solo
            // corta en \n, asi que cada linea queda con un \r colgando al
            // final ("...T _start\r"). El regex de abajo ancla $ justo
            // despues de "_start", asi que ese \r sobrante lo hacia fallar
            // SIEMPRE (no es timing -- por eso los reintentos no ayudaban).
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            std::smatch m;
            if (std::regex_match(line, m, start_line_re)) {
                start_address = static_cast<std::uint32_t>(std::strtoul(m[1].str().c_str(), nullptr, 16));
                found_start = true;
                break;
            }
        }

        if (found_start) {
            break;
        }
        if (attempt < kMaxLinkAttempts) {
            std::cout << "ava_cli build --target barekernel: _start no aparecio en el intento "
                      << attempt << " (probable timing de archivo en Windows) -- reintentando ...\n";
            std::this_thread::sleep_for(kLinkRetryDelay);
        }
    }

    if (!found_start) {
        std::cerr << "error: no se encontro el simbolo _start en la salida de i686-elf-nm sobre "
                  << elf_path.string() << " tras " << kMaxLinkAttempts << " intentos -- revisar que "
                     "main_barekernel.cpp siga exportandolo.\n";
        return 1;
    }

    constexpr std::uint32_t kAppBaseAddress = 0x40000000u;
    if (start_address < kAppBaseAddress) {
        std::cerr << "error: _start (0x" << std::hex << start_address << std::dec
                  << ") esta antes de la base de carga esperada (0x40000000) -- algo cambio en "
                     "app.ld o en como objcopy aplano las secciones, no asumir el offset.\n";
        return 1;
    }
    std::uint32_t entry_offset = start_address - kAppBaseAddress;

    // bss_size ya NO tiene un default hardcodeado del lado de
    // ava_apphdr_writer (ver apphdr_writer.h / apphdr_cli/main.cpp,
    // elf32_bss.h) -- por default se calcula automaticamente leyendo la
    // tabla de secciones de elf_path (el mismo .elf sin aplanar que ya
    // usamos arriba para sacar _start con nm), asi que SIEMPRE lo
    // pasamos. --bss-size del usuario de ava_cli (opts.bss_size) sigue
    // siendo un override manual disponible para casos raros, pero ya no
    // es necesario para el caso comun -- el calculo automatico es correcto
    // por construccion (no depende de que nadie recuerde actualizar un
    // numero a mano cuando el .bss real del programa crece).
    std::vector<std::string> apphdr_args = {
        "--bin", bin_path.string(),
        "--elf", elf_path.string(),
        "--entry-offset", "0x" + [&]{ std::ostringstream o; o << std::hex << entry_offset; return o.str(); }(),
        "--out", out_path_abs.string(),
    };
    if (opts.stack_size) {
        apphdr_args.push_back("--stack-size");
        apphdr_args.push_back(std::to_string(*opts.stack_size));
    }
    if (opts.bss_size) {
        apphdr_args.push_back("--bss-size");
        apphdr_args.push_back(std::to_string(*opts.bss_size));
        std::cout << "ava_cli build --target barekernel: --bss-size " << *opts.bss_size
                  << " explicito -- se ignora el auto-calculo desde " << elf_path.string() << "\n";
    }
    fs::create_directories(out_path_abs.parent_path(), ec);
    std::cout << "ava_cli build --target barekernel: escribiendo AppHeader (entry_offset=0x"
              << std::hex << entry_offset << std::dec << ", bss auto-calculado desde "
              << elf_path.filename().string() << ") ...\n";
    if (!RunTool(process, apphdr_bin->string(), apphdr_args, "ava_apphdr_writer")) {
        return 1;
    }

    fs::path cross_so = cross_build_dir / "libavalang.so";
    if (fs::exists(cross_so, ec)) {
        fs::path so_dest = out_path_abs.parent_path() / "libavalang.so";
        fs::copy_file(cross_so, so_dest, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            std::cout << "[info] libavalang.so (cruzado) copiada a " << so_dest.string()
                      << " -- va en /system/lib/ del disco del kernel (Fase B3), no al lado del "
                         ".exe en /apps/.\n";
        }
    }

    std::cout << "ava_cli build --target barekernel: listo -> " << out_path_abs.string() << "\n";
    std::cout << "  (NO validado contra litekernel real/QEMU en este build -- eso es Fase B3. "
                 "Este comando corrio herramientas reales con argumentos reales; si algo de esto "
                 "fallo, el mensaje de arriba es sobre esa herramienta real, no un mock.)\n";
    return 0;
}

} // namespace

int RunBuildCommand(int argc, char** argv) {
    BuildOptions opts;
    std::string err;
    if (!ParseBuildArgs(argc, argv, opts, err)) {
        std::cerr << "ava_cli build: " << err << "\n";
        PrintBuildUsage();
        return 1;
    }

    fs::path key_file_abs;
    if (!opts.key_file.empty()) {
        std::error_code key_ec;
        key_file_abs = fs::absolute(fs::path(opts.key_file), key_ec);
        if (!fs::exists(key_file_abs, key_ec)) {
            std::cerr << "error: --key-file does not exist: " << opts.key_file << "\n";
            return 1;
        }
    }

    fs::path sign_pfx_abs;
    if (!opts.sign_pfx.empty()) {
#if !defined(_WIN32)
        std::cerr << "error: --sign-pfx is only supported on Windows (uses signtool)\n";
        return 1;
#else
        std::error_code sign_ec;
        sign_pfx_abs = fs::absolute(fs::path(opts.sign_pfx), sign_ec);
        if (!fs::exists(sign_pfx_abs, sign_ec)) {
            std::cerr << "error: --sign-pfx does not exist: " << opts.sign_pfx << "\n";
            return 1;
        }
#endif
    }

    // Fase 9: repo_root se calcula igual que siempre, pero la validacion de
    // "esto es un checkout de AvaLang" (CMakeLists.txt/runtime/avapack/) se
    // movio mas abajo -- el camino rapido (avapack_stub.exe + avapack_gen.exe
    // prebuilt junto a ava_cli.exe, ver TryFastPackWithPrebuiltStub) no
    // necesita el repo para nada salvo --extra-modules-dir (opcional), asi
    // que ya no tiene sentido exigirlo aca antes de siquiera intentarlo.
    fs::path repo_root = opts.repo_root.empty() ? fs::current_path() : fs::path(opts.repo_root);
    std::error_code ec;
    repo_root = fs::absolute(repo_root, ec);

    fs::path project_dir_abs = fs::absolute(fs::path(opts.project_dir), ec);
    if (!fs::exists(project_dir_abs) || !fs::is_directory(project_dir_abs)) {
        std::cerr << "error: --project does not exist or is not a directory: "
                  << opts.project_dir << "\n";
        return 1;
    }

    fs::path raw_out(opts.out_path);
    bool out_is_dir = fs::is_directory(raw_out, ec);
    if (!out_is_dir && !opts.out_path.empty()) {
        char last = opts.out_path.back();
        if (last == '/' || last == '\\') out_is_dir = true;
    }

    fs::path out_path_abs;
    if (out_is_dir) {
        std::string default_name = fs::path(opts.entry_file).stem().string();
        if (default_name.empty()) default_name = "packaged";
        std::string suffix = (opts.target == "barekernel") ? ".exe" : AVACLI_EXE_SUFFIX;
        out_path_abs = fs::absolute(raw_out, ec) / (default_name + suffix);
        std::cout << "[info] --out is a directory -- the packaged binary will be saved as "
                  << out_path_abs.string() << "\n";
    } else {
        out_path_abs = fs::absolute(raw_out, ec);
    }

    fs::create_directories(out_path_abs.parent_path(), ec);
    if (ec) {
        std::cerr << "error: could not create output directory "
                  << out_path_abs.parent_path().string() << " (" << ec.message() << ")\n"
                  << "       (if a FILE with that same name already exists there from a "
                     "previous run, delete it first)\n";
        return 1;
    }

    auto platform = ava::platform::Platform::Create();
    ava::platform::IProcess& process = platform->Process();

    if (!opts.compiler_path.empty()) {
        fs::path compiler_path_abs = fs::absolute(fs::path(opts.compiler_path), ec);
        if (!fs::exists(compiler_path_abs, ec)) {
            std::cerr << "error: --compiler-path does not exist: " << opts.compiler_path << "\n";
            return 1;
        }
#if defined(_WIN32)
        const char path_sep = ';';
#else
        const char path_sep = ':';
#endif
        auto& environment = platform->Environment();
        std::string current_path;
        environment.GetEnvVar("PATH", current_path);
        const std::string new_path = compiler_path_abs.string() +
            (current_path.empty() ? std::string() : (path_sep + current_path));
        environment.SetEnvVar("PATH", new_path);
        std::cout << "[info] --compiler-path: prepended '" << compiler_path_abs.string()
                  << "' to PATH for this build (cmake and any compiler/tool found there "
                     "take priority over the rest of PATH).\n";
    }

    if (opts.target == "barekernel") {
        return RunBuildBarekernelCommand(process, opts, repo_root, project_dir_abs, out_path_abs);
    }

    // Fase 9: camino rapido sin repo/CMake. Si avapack_stub.exe/avapack_gen.exe
    // estan prebuilt junto a ava_cli.exe (ver scripts/build_pack_tools.bat),
    // esto empaqueta el proyecto SIN necesitar el repo de AvaLang al lado
    // (ver TryFastPackWithPrebuiltStub arriba y runtime/avapack/README.md,
    // seccion Fase 9). Si el camino no aplica (--zero-disk, o no hay
    // herramientas prebuilt), cae al flujo de siempre con CMake mas abajo.
    {
        bool fast_ok = false;
        if (TryFastPackWithPrebuiltStub(process, opts, project_dir_abs, key_file_abs,
                                         sign_pfx_abs, out_path_abs, repo_root, fast_ok)) {
            return fast_ok ? 0 : 1;
        }
    }

    // A partir de aca SI hace falta un checkout completo de AvaLang (el
    // camino rapido de arriba no aplico) -- ahora es donde vale la pena
    // validar "esto es un repo de AvaLang", en vez de dejar que CMake
    // falle mas abajo con un error generico.
    fs::path avapack_cmake_lists = repo_root / "runtime" / "avapack" / "CMakeLists.txt";
    if (!fs::exists(avapack_cmake_lists, ec)) {
        std::cerr << "error: no se encontraron avapack_stub.exe/avapack_gen.exe prebuilt junto a "
                  << "ava_cli.exe (generalos con scripts/build_pack_tools.bat) y tampoco hay un "
                  << "checkout completo de AvaLang en " << repo_root.string() << " (falta "
                  << avapack_cmake_lists.string() << ").\n"
                  << "       Usa --repo-root para apuntar a un checkout de AvaLang, o coloca "
                  << "avapack_stub.exe/avapack_gen.exe junto a ava_cli.exe para empacar sin "
                  << "necesitar el repo.\n";
        return 1;
    }

    fs::path build_dir = repo_root / "build_pack";
    const std::string target_name = "avapack_build";
    const std::string build_config = opts.debug_unencrypted ? "Debug" : "Release";

    if (opts.clean_pack && fs::exists(build_dir)) {
        std::cout << "ava_cli build: --clean -- removing " << build_dir.string() << " ...\n";
        fs::remove_all(build_dir, ec);
    }

    std::vector<std::string> configure_args = {
        "-S", repo_root.string(),
        "-B", build_dir.string(),
        "-DAVA_BUILD_PACK=ON",
        "-DAVA_BUILD_CLI=OFF",
        "-DAVA_BUILD_STUDIO=OFF",
        "-DAVA_BUILD_AVAHOST=OFF",
        "-DCMAKE_BUILD_TYPE=" + build_config,
        "-DAVAPACK_PROJECT_DIR=" + project_dir_abs.string(),
        "-DAVAPACK_ENTRY_FILE=" + opts.entry_file,
        "-DAVAPACK_OUT_NAME=" + target_name,
    };

    fs::path prebuilt_dir = GetSelfExecutableDir();
    bool have_prebuilt = !prebuilt_dir.empty()
        && fs::exists(prebuilt_dir / "avalang.dll")
        && fs::exists(prebuilt_dir / "avalang.lib")
        && fs::exists(prebuilt_dir / "avalang_ui.dll")
        && fs::exists(prebuilt_dir / "avalang_ui.lib");
    if (have_prebuilt) {
        configure_args.push_back("-DAVA_PACK_USE_PREBUILT_AVALANG=ON");
        configure_args.push_back("-DAVA_PREBUILT_AVALANG_DIR=" + prebuilt_dir.string());
    } else {
        std::cout << "[info] no se encontraron avalang.dll/avalang.lib/avalang_ui.dll/"
                     "avalang_ui.lib prebuilt junto a ava_cli.exe -- se compilara avalang "
                     "desde fuente en build_pack (mas lento).\n";
    }
    if (!key_file_abs.empty()) {
        configure_args.push_back("-DAVAPACK_KEY_FILE=" + key_file_abs.string());
    }
    fs::path libraries_dir = repo_root / "libraries";
    if (fs::exists(libraries_dir, ec) && fs::is_directory(libraries_dir, ec)) {
        configure_args.push_back("-DAVAPACK_EXTRA_MODULES_DIR=" + libraries_dir.string());
    }
    if (opts.debug_unencrypted) {
        configure_args.push_back("-DAVAPACK_DEBUG_UNENCRYPTED=ON");
        std::cout << "[info] --debug is active: the embedded content will be UNENCRYPTED. Do "
                     "not distribute this .exe.\n";
    }
    if (opts.obfuscate) {
        configure_args.push_back("-DAVAPACK_OBFUSCATE=ON");
        if (opts.obfuscate_strings) configure_args.push_back("-DAVAPACK_OBFUSCATE_STRINGS=ON");
        if (opts.flatten_control_flow) configure_args.push_back("-DAVAPACK_FLATTEN_CONTROL_FLOW=ON");
        std::cout << "[info] --obfuscate is active: the entry is embedded as .avbc bytecode "
                     "instead of .ava source (see runtime/avapack/README.md).\n";
    }
    if (opts.zero_disk) {
        configure_args.push_back("-DAVAPACK_ZERO_DISK=ON");
        std::cout << "[info] --zero-disk is active: the packaged project is resolved against an "
                     "in-memory virtual filesystem, without a temp dir (see "
                     "runtime/avapack/README.md).\n";
    }
    if (const char* vcpkg_root = std::getenv("VCPKG_ROOT")) {
        fs::path toolchain = fs::path(vcpkg_root) / "scripts" / "buildsystems" / "vcpkg.cmake";
        configure_args.push_back("-DCMAKE_TOOLCHAIN_FILE=" + toolchain.string());
        if (const char* triplet = std::getenv("AVA_VCPKG_TRIPLET")) {
            configure_args.push_back(std::string("-DVCPKG_TARGET_TRIPLET=") + triplet);
        }
    } else {
        std::cout << "[info] VCPKG_ROOT is not set -- if avalang.dll needs real ANTLR4, run "
                     "install.bat first (see the ava_cli README).\n";
    }

    std::cout << "ava_cli build: configuring (" << build_dir.string() << ") ...\n";
    if (!RunTool(process, "cmake", configure_args, "cmake (configure)")) {
        return 1;
    }

    std::vector<std::string> build_args = {
        "--build", build_dir.string(),
        "--target", target_name,
        "--config", build_config,
        "--parallel",
    };
    std::cout << "ava_cli build: compiling " << target_name << " ...\n";
    if (!RunTool(process, "cmake", build_args, "cmake --build")) {
        std::cerr << "error: the build failed -- if the packaged project has an .ava file that "
                     "does not compile, the avalang error message should be above.\n";
        return 1;
    }

    std::optional<fs::path> built = FindBuiltBinary(build_dir, target_name, build_config);
    if (!built) {
        std::cerr << "error: the build seemed to succeed but " << target_name << AVACLI_EXE_SUFFIX
                  << " was not found under " << build_dir.string()
                  << "/runtime/avalang/ -- check the output above.\n";
        return 1;
    }

    fs::copy_file(*built, out_path_abs, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "error: could not copy the binary to " << out_path_abs.string()
                  << " (" << ec.message() << ")\n";
        return 1;
    }

    fs::path built_dir = built->parent_path();
    fs::path out_dir = out_path_abs.parent_path();
    CopyRuntimeDllIfPresent(built_dir, out_dir, "avalang.dll");
    CopyAvaUiDllIfAvailable(built_dir, out_dir, repo_root, build_config);
    if (fs::exists(libraries_dir, ec) && fs::is_directory(libraries_dir, ec)) {
        CopyExternNativeLibraries(project_dir_abs, libraries_dir, out_dir);
    }

    if (opts.obfuscate) {
        fs::path avmap_src = build_dir / "runtime" / "avapack" / "generated" / "embedded_project.avmap";
        if (fs::exists(avmap_src, ec)) {
            fs::path avmap_dst = out_path_abs;
            avmap_dst.replace_extension(".avmap");
            fs::copy_file(avmap_src, avmap_dst, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                std::cout << "[info] SymbolMap saved to " << avmap_dst.string()
                          << " -- do NOT distribute it alongside the .exe.\n";
            } else {
                std::cerr << "warning: could not copy .avmap to " << avmap_dst.string() << "\n";
            }
        }
    }

    if (!sign_pfx_abs.empty()) {
        if (!SignBinary(process, out_path_abs, sign_pfx_abs.string(), opts.sign_password_env,
                         opts.sign_timestamp_url)) {
            std::cerr << "warning: code signing failed -- " << out_path_abs.string()
                      << " remains unsigned (the rest of the build succeeded).\n";
        }
    }

    std::cout << "ava_cli build: done -> " << out_path_abs.string() << "\n";
    std::cout << "  (build_pack/ kept at " << build_dir.string()
               << " for faster incremental builds -- use --clean for a fresh one)\n";
    return 0;
}