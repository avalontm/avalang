#include "cli_commands.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "config/app_config.h"
#include "core/crash_handler.h"
#include "core/host_options.h"
#include "core/logger.h"
#include "plugin/plugin_loader.h"
#include "runtime/runtime_host.h"
#include "web/server/app.h"
#include "web/routing/router.h"

#ifdef AVAHOST_HAS_UI_PIPELINE
#include "rendering/ui_pipeline_static_renderer.h"
#include "rendering/ui_pipeline_dynamic_renderer.h"
#endif

namespace fs = std::filesystem;

namespace avahost {

namespace {

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

    if (!portOverride.empty()) options.port = std::atoi(portOverride.c_str());
    if (!hostOverride.empty()) options.host = hostOverride;

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
        "  avahost render-static <file.avaui> [output.html]\n"
        "  avahost render-dynamic <file.avaui> [output.html] [--project <dir>]\n"
        "                         Fase 20.1: renders one .avaui file through the\n"
        "                         new avalang.ui pipeline (no state/code binding).\n"
        "                         Prints to stdout if [output.html] is omitted.\n\n"
        "Options:\n"
        "  --project <dir>        Project root (default: current directory)\n"
        "  --host <host>          Override appsettings.json host\n"
        "  --port <port>          Override appsettings.json port";
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

    WriteFile(root / "app.ava",
        "# Recursos globales de la app -- se cargan en <head> de todas\n"
        "# las páginas .avaui. Vacío por defecto: AvaUI no depende de\n"
        "# CSS/Tailwind para renderizar, usa primitivas nativas de\n"
        "# layout (row, column, padding, gap, align, width, height).\n"
        "# Si igual necesitás CSS propio o una librería JS externa,\n"
        "# declarala acá a mano, por ejemplo:\n"
        "#   import \"css/app.css\"\n"
        "#   import \"js/some-lib.js\"\n");

    WriteFile(root / "routes" / "index.avaui",
        "extends layouts.main\n\n"
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
        "        padding = 32\n"
        "        gap = 16\n\n"
        "        text\n"
        "            text = \"Welcome to AvaHost\"\n"
        "            fontSize = 24\n"
        "        end\n"
        "        text\n"
        "            text = \"Edit routes/index.avaui to get started.\"\n"
        "            textColor = \"6B7280\"\n"
        "        end\n"
        "    end\n"
        "end\n\n"
        "methods\n"
        "    # Handlers y funciones\n"
        "end\n");

    WriteFile(root / "layouts" / "main.avaui",
        "view\n"
        "    column\n"
        "        row\n"
        "            padding = 16\n"
        "            gap = 16\n"
        "            align = \"center\"\n"
        "            borderWidth = 1\n"
        "            borderColor = \"E5E7EB\"\n\n"
        "            text\n"
        "                text = \"AvaHost\"\n"
        "                fontSize = 18\n"
        "            end\n"
        "            link\n"
        "                text = \"Home\"\n"
        "                href = \"/\"\n"
        "                textColor = \"2563EB\"\n"
        "            end\n"
        "        end\n\n"
        "        slot\n\n"
        "        row\n"
        "            padding = 16\n"
        "            borderWidth = 1\n"
        "            borderColor = \"E5E7EB\"\n\n"
        "            text\n"
        "                text = \"AvaHost 0.1\"\n"
        "                fontSize = 13\n"
        "                textColor = \"6B7280\"\n"
        "            end\n"
        "        end\n"
        "    end\n"
        "end\n");

    WriteFile(root / "wwwroot" / "css" / "app.css",
        "/* Project-specific CSS -- not loaded by default.\n"
        "   AvaUI styles and sizes components natively, through the\n"
        "   LayoutEngine's own properties (row/column, padding, gap,\n"
        "   align, width, height, borderWidth/borderColor, fontSize,\n"
        "   textColor, backgroundColor), rendered identically by\n"
        "   GdiRenderer (desktop) and HTMLRenderer (web). It does not\n"
        "   use Tailwind or hand-written CSS -- see\n"
        "   docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md.\n"
        "\n"
        "   This file exists only as a place for CSS you explicitly opt\n"
        "   into by adding `import \"css/app.css\"` to app.ava -- it is\n"
        "   not linked automatically. */\n");

    fs::create_directories(root / "layouts");
    fs::create_directories(root / "components");
    fs::create_directories(root / "services");
    fs::create_directories(root / "models");
    fs::create_directories(root / "plugins");

    std::cout << "Created new AvaHost project at " << fs::absolute(root).string() << "\n";
    return 0;
}

namespace {

Logger& SetupErrorLoggingAndCrashHandlers(const HostOptions& options) {
    std::string logPath = (fs::path(options.projectRoot) / "avahost-error.log").string();
    EnableErrorFileLogging(logPath);
    Logger& logger = GlobalLogger();
    InstallCrashHandlers(logger);
    logger.Info("errors will also be written to " + logPath);
    return logger;
}
} // namespace

int CmdRun(const std::vector<std::string>& args) {
    HostOptions options = LoadProjectOptions(args);
    Logger& logger = SetupErrorLoggingAndCrashHandlers(options);

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
    Logger& logger = SetupErrorLoggingAndCrashHandlers(options);

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
            std::string error;
            if (!runtime.ValidateAvaUiFile(contents.str(), error)) {
                logger.Error("build: " + match.filePath + ": " + error);
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

int CmdRenderStatic(const std::vector<std::string>& args) {
#ifdef AVAHOST_HAS_UI_PIPELINE
    if (args.empty()) {
        std::cerr << "avahost render-static: missing <file.avaui>\n\n"
                  << "Usage: avahost render-static <file.avaui> [output.html]\n";
        return 1;
    }

    fs::path inputPath = args[0];
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "avahost render-static: cannot open '" << inputPath.string() << "'\n";
        return 1;
    }
    std::ostringstream sourceBuf;
    sourceBuf << in.rdbuf();

    UiPipelineRenderOptions renderOptions;
    std::string html, error;
    if (!RenderAvauiStatic(sourceBuf.str(), renderOptions, html, error)) {
        std::cerr << "avahost render-static: " << error << "\n";
        return 1;
    }

    if (args.size() >= 2) {
        fs::path outputPath = args[1];
        std::ofstream out(outputPath, std::ios::binary);
        if (!out) {
            std::cerr << "avahost render-static: cannot write '" << outputPath.string() << "'\n";
            return 1;
        }
        out << html;
        std::cout << "avahost render-static: wrote " << html.size() << " bytes to '"
                  << outputPath.string() << "'\n";
    } else {
        std::cout << html;
    }

    return 0;
#else
    (void)args;
    std::cerr << "avahost render-static: not available -- this build was configured "
                 "with AVA_BUILD_UI=OFF (Fase 20). Reconfigure with -DAVA_BUILD_UI=ON "
                 "and rebuild to enable it.\n";
    return 1;
#endif
}

int CmdRenderDynamic(const std::vector<std::string>& args) {
#ifdef AVAHOST_HAS_UI_PIPELINE
    std::vector<std::string> positional;
    std::string projectRoot = ".";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--project" && i + 1 < args.size()) {
            projectRoot = args[++i];
        } else {
            positional.push_back(args[i]);
        }
    }

    if (positional.empty()) {
        std::cerr << "avahost render-dynamic: missing <file.avaui>\n\n"
                  << "Usage: avahost render-dynamic <file.avaui> [output.html] [--project <dir>]\n";
        return 1;
    }

    fs::path inputPath = positional[0];
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "avahost render-dynamic: cannot open '" << inputPath.string() << "'\n";
        return 1;
    }
    std::ostringstream sourceBuf;
    sourceBuf << in.rdbuf();

    RuntimeHost host;
    host.SetCurrentDir(projectRoot);
    host.AddSearchPath(projectRoot);

    UiPipelineRenderOptions renderOptions;
    std::string html, error;
    if (!RenderAvauiDynamic(host, sourceBuf.str(), renderOptions, html, error, inputPath.string())) {
        std::cerr << "avahost render-dynamic: " << error << "\n";
        return 1;
    }

    if (positional.size() >= 2) {
        fs::path outputPath = positional[1];
        std::ofstream out(outputPath, std::ios::binary);
        if (!out) {
            std::cerr << "avahost render-dynamic: cannot write '" << outputPath.string() << "'\n";
            return 1;
        }
        out << html;
        std::cout << "avahost render-dynamic: wrote " << html.size() << " bytes to '"
                  << outputPath.string() << "'\n";
    } else {
        std::cout << html;
    }

    return 0;
#else
    (void)args;
    std::cerr << "avahost render-dynamic: not available -- this build was configured "
                 "with AVA_BUILD_UI=OFF (Fase 20). Reconfigure with -DAVA_BUILD_UI=ON "
                 "and rebuild to enable it.\n";
    return 1;
#endif
}

} // namespace avahost