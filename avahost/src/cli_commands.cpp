#include "cli_commands.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "config/app_config.h"
#include "core/host_options.h"
#include "core/logger.h"
#include "plugin/plugin_loader.h"
#include "runtime/runtime_host.h"
#include "web/app.h"
#include "web/router.h"

namespace fs = std::filesystem;

namespace avahost {

namespace {

// Loads appsettings.json from the current directory (or --project if
// given) into a HostOptions, applying any --port/--host overrides.
// Shared by run/watch/build/doctor so they all see the same project.
HostOptions LoadProjectOptions(const std::vector<std::string>& args) {
    HostOptions options;
    options.projectRoot = ".";

    std::string portOverride;
    std::string hostOverride;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--project" && i + 1 < args.size()) {
            options.projectRoot = args[++i];
        } else if (args[i] == "--port" && i + 1 < args.size()) {
            portOverride = args[++i];
        } else if (args[i] == "--host" && i + 1 < args.size()) {
            hostOverride = args[++i];
        }
    }

    std::string configError;
    if (!AppConfig::Load(options, configError)) {
        GlobalLogger().Warn("appsettings.json: " + configError);
    }

    // CLI flags win over appsettings.json, applied after the config
    // load so they can't be silently overwritten by it.
    if (!portOverride.empty()) options.port = std::atoi(portOverride.c_str());
    if (!hostOverride.empty()) options.host = hostOverride;

    // Directory options in HostOptions are relative to projectRoot.
    auto resolve = [&](std::string& dir) {
        fs::path p = fs::path(options.projectRoot) / dir;
        dir = p.string();
    };
    resolve(options.routesDir);
    resolve(options.wwwrootDir);
    resolve(options.layoutsDir);
    resolve(options.componentsDir);
    resolve(options.pluginsDir);

    return options;
}

void WriteFile(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << contents;
}

} // namespace

void PrintUsage() {
    std::cout <<
        "AvaHost -- application hosting platform for the AvaLang ecosystem\n\n"
        "Usage:\n"
        "  avahost new [name]     Creates a new project (default: current dir)\n"
        "  avahost run            Runs the current project\n"
        "  avahost watch          Runs the current project with Hot Reload\n"
        "  avahost build          Validates/compiles the project's routes\n"
        "  avahost publish        Publishes a deployable application\n"
        "  avahost doctor         Runs diagnostics on the current project\n\n"
        "Options:\n"
        "  --project <dir>        Project root (default: current directory)\n"
        "  --host <host>          Override appsettings.json host\n"
        "  --port <port>          Override appsettings.json port\n";
}

int CmdNew(const std::vector<std::string>& args) {
    std::string name = args.empty() ? "." : args[0];
    fs::path root = (name == ".") ? fs::current_path() : fs::path(name);

    WriteFile(root / "appsettings.json",
        "{\n"
        "    \"host\": \"localhost\",\n"
        "    \"port\": 8080,\n"
        "    \"environment\": \"Development\",\n"
        "    \"watch\": true,\n"
        "\n"
        "    \"routesDir\": \"routes\",\n"
        "    \"wwwrootDir\": \"wwwroot\",\n"
        "    \"layoutsDir\": \"layouts\",\n"
        "    \"componentsDir\": \"components\",\n"
        "    \"pluginsDir\": \"plugins\"\n"
        "}\n");

    WriteFile(root / "routes" / "index.avaui",
        "extends \"main\"\n\n"
        "properties\n"
        "    # Variables generales del componente para usar en código\n"
        "end\n\n"
        "metadata\n"
        "    title = \"Welcome to AvaHost\"\n"
        "    description = \"A new app built with AvaHost.\"\n"
        "end\n\n"
        "state\n"
        "    # Variables de estado reactivas\n"
        "end\n\n"
        "view\n"
        "    column\n"
        "        class = \"flex flex-col gap-4 p-8\"\n\n"
        "        text\n"
        "            text = \"Welcome to AvaHost\"\n"
        "            class = \"text-2xl font-bold\"\n"
        "        end\n"
        "        text\n"
        "            text = \"Edit routes/index.avaui to get started.\"\n"
        "            class = \"text-gray-600\"\n"
        "        end\n"
        "    end\n"
        "end\n\n"
        "methods\n"
        "    # Handlers y funciones\n"
        "end\n");

    // layouts/main.avaui -- the shell every `extends "main"` page
    // renders into (docs/architecture/17_AVAUI_FILE_FORMAT.md,
    // "extends"). `slot` marks where the page's own view is inserted;
    // navbar/footer here render on every page without each page
    // repeating them. Uses Tailwind utility classes directly (via the
    // `class` property every component supports) instead of relying
    // on app.css, which now ships blank.
    WriteFile(root / "layouts" / "main.avaui",
        "view\n"
        "    column\n"
        "        class = \"min-h-screen flex flex-col\"\n\n"
        "        row\n"
        "            class = \"flex items-center justify-between p-4 border-b border-gray-200\"\n\n"
        "            text\n"
        "                text = \"AvaHost\"\n"
        "                class = \"font-semibold text-lg\"\n"
        "            end\n"
        "            link\n"
        "                text = \"Home\"\n"
        "                href = \"/\"\n"
        "                class = \"text-blue-600 hover:underline\"\n"
        "            end\n"
        "        end\n\n"
        "        slot\n\n"
        "        row\n"
        "            class = \"p-4 border-t border-gray-200 text-sm text-gray-500\"\n\n"
        "            text\n"
        "                text = \"AvaHost 0.1\"\n"
        "            end\n"
        "        end\n"
        "    end\n"
        "end\n");

    WriteFile(root / "wwwroot" / "css" / "app.css",
        "/* Project-specific CSS.\n"
        "   Tailwind and other CSS is loaded via <link> tags in layouts.\n"
        "   AvaLang styling uses Tailwind utility classes directly via the `class`\n"
        "   property on any .avaui component, e.g.:\n"
        "\n"
        "       column\n"
        "           class = \"flex flex-col gap-4 p-6\"\n"
        "       end\n"
        "\n"
        "   Add project-wide CSS overrides or custom rules below. */\n");

    fs::create_directories(root / "layouts");
    fs::create_directories(root / "components");
    fs::create_directories(root / "services");
    fs::create_directories(root / "models");
    fs::create_directories(root / "plugins");

    std::cout << "Created new AvaHost project at " << fs::absolute(root).string() << "\n";
    return 0;
}

int CmdRun(const std::vector<std::string>& args) {
    HostOptions options = LoadProjectOptions(args);
    Logger& logger = GlobalLogger();

    logger.Info("AvaHost starting (" + options.environment + ") -- project: " +
                fs::absolute(options.projectRoot).string());

    if (options.watch) {
        logger.Info("AvaHost Hot Reload enabled (appsettings.json \"watch\": true) -- project: " +
                    fs::absolute(options.projectRoot).string());
    }

    AvaHostApp app(options, logger);
    return app.Run() ? 0 : 1;
}

int CmdWatch(const std::vector<std::string>& args) {
    HostOptions options = LoadProjectOptions(args);
    options.watch = true;
    Logger& logger = GlobalLogger();

    logger.Info("AvaHost Hot Reload enabled -- project: " +
                fs::absolute(options.projectRoot).string());

    AvaHostApp app(options, logger);
    return app.Run() ? 0 : 1;
}

int CmdBuild(const std::vector<std::string>& args) {
    HostOptions options = LoadProjectOptions(args);
    Logger& logger = GlobalLogger();

    RuntimeHost runtime;
    runtime.SetCurrentDir(options.projectRoot);
    runtime.AddSearchPath(options.routesDir);
    Router router(options.routesDir, runtime);

    int errorCount = 0;
    for (const auto& match : router.AllRouteFiles()) {
        std::ifstream file(match.filePath, std::ios::binary);
        if (!file) {
            logger.Error("build: could not read " + match.filePath);
            ++errorCount;
            continue;
        }
        std::ostringstream contents;
        contents << file.rdbuf();

        if (match.isAvaUi) {
            auto doc = runtime.ParseAvaUiFile(contents.str());
            if (!doc.ok) {
                logger.Error("build: " + match.filePath + ": " + doc.error);
                ++errorCount;
            } else {
                logger.Info("build: OK " + match.filePath);
            }
        } else {
            std::string error;
            if (!runtime.RunScript(contents.str(), match.filePath, error)) {
                logger.Error("build: " + match.filePath + ": " + error);
                ++errorCount;
            } else {
                logger.Info("build: OK " + match.filePath);
            }
        }
    }

    if (errorCount == 0) {
        logger.Info("build: succeeded, no errors");
        return 0;
    }
    logger.Error("build: failed with " + std::to_string(errorCount) + " error(s)");
    return 1;
}

int CmdPublish(const std::vector<std::string>& /*args*/) {
    // Explicit stub: publish depends on the bytecode cache (plan
    // section 14 / roadmap v0.4 "Publish"), which doesn't exist yet --
    // `ava_compile` output (AvaModule*) isn't serializable today. This
    // command intentionally fails loudly instead of silently doing a
    // partial/incorrect publish.
    std::cerr << "avahost publish: not implemented yet.\n"
                 "  Publish requires the bytecode cache described in\n"
                 "  docs/architecture/AVAHOST_IMPLEMENTATION_PLAN.md section 14,\n"
                 "  planned for v0.4. Use 'avahost run' for now.\n";
    return 1;
}

int CmdDoctor(const std::vector<std::string>& args) {
    HostOptions options = LoadProjectOptions(args);
    Logger& logger = GlobalLogger();

    std::cout << "AvaHost doctor\n";
    std::cout << "  project root : " << fs::absolute(options.projectRoot).string() << "\n";
    std::cout << "  environment  : " << options.environment << "\n";
    std::cout << "  host:port    : " << options.host << ":" << options.port << "\n";

    std::cout << "  appsettings.json : "
              << (fs::exists(fs::path(options.projectRoot) / "appsettings.json") ? "found" : "missing (using defaults)")
              << "\n";

    std::cout << "  routes dir   : " << options.routesDir
              << (fs::exists(options.routesDir) ? "" : "  (missing)") << "\n";
    RuntimeHost routerRuntime;
    Router router(options.routesDir, routerRuntime);
    auto routes = router.ListRoutes();
    std::cout << "  routes found : " << routes.size() << "\n";
    for (const auto& r : routes) std::cout << "    " << r << "\n";

    std::cout << "  wwwroot dir  : " << options.wwwrootDir
              << (fs::exists(options.wwwrootDir) ? "" : "  (missing)") << "\n";

    std::cout << "  plugins dir  : " << options.pluginsDir
              << (fs::exists(options.pluginsDir) ? "" : "  (missing)") << "\n";
    PluginLoader plugins;
    plugins.LoadAll(options.pluginsDir, options, logger);
    std::cout << "  plugins loaded: " << plugins.Count() << "\n";
    for (const auto& name : plugins.Names()) std::cout << "    " << name << "\n";
    plugins.UnloadAll();

    return 0;
}

} // namespace avahost