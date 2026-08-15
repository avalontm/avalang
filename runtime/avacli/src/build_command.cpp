#include "build_command.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"

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

// Carpeta donde vive el propio ava_cli.exe en ejecucion -- en build_cli\ esa
// carpeta ya tiene avalang.dll/avalang.lib/avalang_ui.dll/avalang_ui.lib (ver
// RUNTIME_OUTPUT_DIRECTORY/ARCHIVE_OUTPUT_DIRECTORY en runtime/avacli/
// CMakeLists.txt y runtime/avaui/CMakeLists.txt), asi que sirve directo como
// AVA_PREBUILT_AVALANG_DIR para el build_pack de `ava_cli build` (ver mas
// abajo). Devuelve una ruta vacia si no se pudo determinar (best-effort, cae
// de vuelta a compilar avalang desde fuente).
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
        "                certificate expires, even if the binary hasn't changed.\n";
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
    // Stream cmake/msbuild's own output straight to our (now unbuffered,
    // see main.cpp) stdout as it's produced, instead of buffering the
    // whole thing in a ProcessResult and only printing it once the
    // subprocess exits -- cmake configure and especially `cmake --build`
    // (MSBuild compiling avapack_gen/avapack_build) are the slowest part
    // of a package build by far, so this is what actually makes the
    // Build panel's "real time" log real for the bulk of a build's
    // duration, not just for ava_cli's own few progress lines.
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

    // Fallback for backends that don't implement IProcessStream (e.g.
    // the Linux/Mac stubs) -- same blocking behavior as before.
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

// avalang_ui.dll: avalang.dll la necesita para linkear (ver nota de
// AVA_BUILD_UI mas arriba), asi que build_pack ya la compila y el
// RUNTIME_OUTPUT_DIRECTORY de avalang_ui (runtime/avaui/CMakeLists.txt) la
// deja en la misma carpeta que avalang.dll y avapack_gen.exe -- built_binary_dir
// ya deberia tenerla. Este fallback a build_cli\ (build_cli.bat la deja en
// runtime/avaui/<Config>/, ver ese script) queda solo por si built_binary_dir
// no la tiene por algun motivo (p.ej. build_pack quedo en un estado raro
// antes de --clean). Si no esta ni en build_pack ni en build_cli, seguimos
// sin ella -- best effort, sin bloquear el build.
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

    // Espeja vm_extern.cpp::CandidateFileNames: en Windows busca <name>.dll;
    // en Linux busca lib<name>.so y <name>.so (en ese orden, igual que el VM).
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

    fs::path repo_root = opts.repo_root.empty() ? fs::current_path() : fs::path(opts.repo_root);
    std::error_code ec;
    repo_root = fs::absolute(repo_root, ec);

    if (!fs::exists(repo_root / "CMakeLists.txt") ||
        !fs::exists(repo_root / "runtime" / "avapack" / "CMakeLists.txt")) {
        std::cerr << "error: '" << repo_root.string()
                  << "' does not look like the root of the AvaLang repo "
                     "(missing CMakeLists.txt or runtime/avapack/).\n"
                  << "       Run 'ava_cli build' from the repo root, or pass "
                     "--repo-root <path>.\n";
        return 1;
    }

    fs::path project_dir_abs = fs::absolute(fs::path(opts.project_dir), ec);
    if (!fs::exists(project_dir_abs) || !fs::is_directory(project_dir_abs)) {
        std::cerr << "error: --project does not exist or is not a directory: "
                  << opts.project_dir << "\n";
        return 1;
    }

    // --out puede ser el path completo del .exe final (comportamiento
    // original) o, si el usuario pasa una carpeta (existente, o que termina
    // en '/' o '\\'), el nombre del binario se deriva de --entry y se
    // guarda ADENTRO de esa carpeta -- evita el caso confuso de que
    // copy_file falle (o peor, cree un archivo sin extension) cuando el
    // usuario en realidad queria decir "guardalo en esta carpeta".
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
        out_path_abs = fs::absolute(raw_out, ec) / (default_name + AVACLI_EXE_SUFFIX);
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

    fs::path build_dir = repo_root / "build_pack";
    const std::string target_name = "avapack_build";
    const std::string build_config = opts.debug_unencrypted ? "Debug" : "Release";

    // build_pack/ ya NO se borra despues de un build exitoso (ver mas abajo)
    // -- persiste entre corridas a proposito, para que CMake solo recompile
    // lo que realmente cambio (normalmente nada de avalang.dll/avapack_gen,
    // solo el embedded_project.cpp del proyecto empacado). --clean pide
    // explicitamente un arbol de build fresco, ej. si sospechas que quedo
    // en un estado raro.
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
    // avapack_gen y el .exe empacado solo necesitan linkear contra
    // avalang.dll/avalang_ui.dll (ver runtime/avapack/CMakeLists.txt) -- no
    // hay que recompilar avalang/avaui desde fuente en build_pack cada vez
    // que corre `ava_cli build`, ya estan compilados en build_cli\ (la misma
    // carpeta donde vive este propio ava_cli.exe, ver GetSelfExecutableDir
    // mas abajo). AVA_PACK_USE_PREBUILT_AVALANG=ON hace que el CMakeLists.txt
    // raiz declare avalang/avalang_ui como targets IMPORTED en vez de
    // compilarlos (ver ese archivo). Si por algun motivo no se puede
    // determinar esa carpeta (o le faltan los binarios), caemos de vuelta a
    // compilar avalang/avaui desde fuente -- mas lento, pero siempre
    // funciona.
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
