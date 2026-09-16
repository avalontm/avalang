#include "native_app_host.h"

#ifdef _WIN32

#include <windows.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include "components/ComponentTree.h"
#include "parser/AvauiParser.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "theme/ProjectFontOverrides.h"
#include "theme/ProjectStyleOverrides.h"
#include "theme/ProjectAnimationOverrides.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "events/EventDispatcher.h"
#include "navigation/Navigator.h"
#include "platform/contract/IPlatform.h"
#include "platform/windows/GdiRenderer.h"
#include "platform/windows/WinKeyTranslation.h"
#include "perf/StartupProfiler.h"

#include "runtime/runtime_host.h"
#include "rendering/ui_vm_event_bridge.h"
#include "rendering/ui_vm_lifecycle.h"
#include "rendering/ui_vm_state_bridge.h"
#include "web/routing/router.h"

namespace avahost {
namespace native {

namespace {

bool ReadFile(const std::string& path, std::string& outSource, std::string& outError) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        outError = "cannot open " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    outSource = buffer.str();
    return true;
}

bool IsExternalHref(const std::string& href) {
    return href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0 ||
           href.rfind("mailto:", 0) == 0;
}

void OpenExternal(const std::string& url) {
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string RouteFromFilePath(const std::string& filePath, const std::string& routesDir) {
    std::error_code ec;
    std::filesystem::path relative =
        std::filesystem::relative(filePath, routesDir, ec);
    if (ec) {
        return "/";
    }
    relative.replace_extension();
    std::string routePath = "/" + relative.generic_string();

    const std::string suffix = "/index";
    if (routePath.size() >= suffix.size() &&
        routePath.compare(routePath.size() - suffix.size(), suffix.size(), suffix) == 0) {
        routePath = routePath.substr(0, routePath.size() - suffix.size());
        if (routePath.empty()) routePath = "/";
    } else if (routePath == "/index") {
        routePath = "/";
    }
    return routePath;
}

class NavigateClickHandler final : public avalang::ui::events::IEventHandler {
public:
    NavigateClickHandler(std::string href, std::string* outTarget)
        : href_(std::move(href)), outTarget_(outTarget) {}

    void OnEvent(avalang::ui::events::IEvent*) override {
        if (IsExternalHref(href_)) {
            OpenExternal(href_);
            return;
        }
        *outTarget_ = href_;
    }

private:
    std::string href_;
    std::string* outTarget_;
};

void WireHrefNavigation(avalang::ui::IComponent* node,
                         avalang::ui::events::IEventDispatcher& dispatcher,
                         std::vector<std::unique_ptr<NavigateClickHandler>>& handlers,
                         std::string& pendingNavigation) {
    if (!node) return;

    const avalang::ui::PropertyValue* href = node->GetProperty("href");
    if (href && href->Type() == avalang::ui::PropertyType::String && !href->AsString().empty()) {
        auto handler = std::make_unique<NavigateClickHandler>(href->AsString(), &pendingNavigation);
        dispatcher.Subscribe(node->Id(), avalang::ui::events::EventType::Click, handler.get());
        handlers.push_back(std::move(handler));
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        WireHrefNavigation(child, dispatcher, handlers, pendingNavigation);
    }
}

bool WasKeyJustPressed(avalang::ui::platform::IPlatform& platform, int vkCode, bool& wasDown) {
    const bool isDown = platform.Keyboard().IsKeyDown(vkCode);
    const bool justPressed = isDown && !wasDown;
    wasDown = isDown;
    return justPressed;
}

std::string SanitizeViewNameSegment(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) out += c;
    }
    return out;
}

std::string DeriveViewClassName(const std::string& avauiPath) {
    std::string name = SanitizeViewNameSegment(std::filesystem::path(avauiPath).stem().string());
    if (name.empty()) name = "View";
    if (std::isdigit(static_cast<unsigned char>(name.front()))) name = "View" + name;
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return name;
}

}  // namespace

int RunViewLoop(const std::string& avauiPath, int width, int height, std::string& outError) {
    const std::string projectRoot = std::filesystem::path(avauiPath).parent_path().string();
    const std::string routesDir = projectRoot.empty() ? std::string(".") : projectRoot;

    auto coldStartStage = std::make_unique<avalang::ui::perf::ScopedStartupStage>("cold_start");

    avalang::ui::platform::IPlatform& platform = avalang::ui::platform::GetPlatform();
    std::unique_ptr<avalang::ui::platform::AppSurface> surface;
    {
        avalang::ui::perf::ScopedStartupStage stage("surface_create");
        surface.reset(platform.CreateSurface());
        surface->Create(width, height, "AvaUI");
        surface->Show();
    }

    HWND hwnd = static_cast<HWND>(surface->NativeHandle());
    avalang::ui::GdiRenderer renderer(hwnd, width, height);

    avalang::ui::navigation::Navigator navigator;
    navigator.Push(avalang::ui::navigation::Route(RouteFromFilePath(avauiPath, routesDir)));

    std::string currentFilePath = avauiPath;
    bool backKeyWasDown = false;
    bool altLeftWasDown = false;
    bool firstFrame = true;

    while (!surface->IsClosed()) {
        std::string source;
        if (!ReadFile(currentFilePath, source, outError)) return 1;

        std::unique_ptr<avalang::ui::perf::ScopedStartupStage> parseStage;
        if (firstFrame) {
            parseStage = std::make_unique<avalang::ui::perf::ScopedStartupStage>("first_parse");
        }

        avalang::ui::parser::ParsedAvaui parsed;
        try {
            parsed = avalang::ui::parser::AvauiParser::Parse(source, currentFilePath);
        } catch (const std::exception& e) {
            outError = e.what();
            return 1;
        }
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree";
            return 1;
        }
        avalang::ui::IComponent* root = parsed.tree->Root();

        RuntimeHost host;
        if (!projectRoot.empty()) {
            host.SetCurrentDir(projectRoot);
            host.AddSearchPath(projectRoot);
        }

        VmStateBridge stateBridge(host);
        stateBridge.BindWithOverlay(parsed.state, "");

        std::string viewError;
        if (!host.LoadView(DeriveViewClassName(currentFilePath), parsed.code, root, viewError)) {
            outError = "view failed to load: " + viewError;
            return 1;
        }

        std::unique_ptr<avalang::ui::IThemeProvider> themeProvider(
            avalang::ui::CreateDefaultThemeProvider());
        if (!themeProvider) {
            outError = "failed to create the default theme provider";
            return 1;
        }
        avalang::ui::theme::ProjectTheme projectTheme(
            themeProvider->Current(), avalang::ui::theme::LoadProjectFontOverrides(projectRoot));
        projectTheme.RegisterProjectFonts();
        avalang::ui::theme::ProjectStyleSheet projectStyles =
            avalang::ui::theme::LoadProjectStyleOverrides(projectRoot);
        avalang::ui::RenderTheme::Apply(parsed.tree.get(), &projectTheme, &projectStyles);

        std::unique_ptr<avalang::ui::LayoutEngine> layoutEngine = avalang::ui::LayoutEngine::Create();
        layoutEngine->SetTextEvaluator([&stateBridge](const std::string& raw) {
            return stateBridge.EvalIdentifier(raw);
        });

        avalang::ui::events::EventDispatcher dispatcher;
        dispatcher.SetLayoutEngine(layoutEngine.get());
        dispatcher.SetPlatformInput(&platform.Mouse(), &platform.Keyboard());
        dispatcher.SetWheelSource(&platform.Wheel());
        dispatcher.SetTextInputSource(&platform.TextInput());
        dispatcher.SetTouchSource(&platform.Touch());
        dispatcher.SetImeSource(&platform.Ime());
        dispatcher.SetKeyTranslator(&avalang::ui::platform::windows::TranslateVirtualKey);

        WireVmEventHandlers(root, dispatcher, host, stateBridge);

        Router router(routesDir, host);
        std::string pendingNavigation;
        std::vector<std::unique_ptr<NavigateClickHandler>> navHandlers;
        WireHrefNavigation(root, dispatcher, navHandlers, pendingNavigation);

        if (!VmLifecycle::NotifyMount(host, stateBridge, root, outError)) {
            return 1;
        }

        std::string nextFilePath;

        if (firstFrame) {
            parseStage.reset();
            coldStartStage.reset();
            firstFrame = false;
        }

        while (!surface->IsClosed()) {
            surface->ProcessEvents();
            dispatcher.PollInput(root);
            host.PumpAsyncOnce();

            if (!pendingNavigation.empty()) {
                if (auto match = router.Resolve(pendingNavigation)) {
                    nextFilePath = match->filePath;
                    navigator.Push(avalang::ui::navigation::Route(pendingNavigation));
                }
                pendingNavigation.clear();
                if (!nextFilePath.empty()) break;
            }

            const bool backRequested =
                WasKeyJustPressed(platform, VK_BROWSER_BACK, backKeyWasDown) ||
                (platform.Keyboard().IsKeyDown(VK_MENU) &&
                 WasKeyJustPressed(platform, VK_LEFT, altLeftWasDown));
            if (backRequested && navigator.CanBack()) {
                navigator.Back();
                if (const avalang::ui::navigation::Route* current = navigator.Current()) {
                    if (auto match = router.Resolve(current->Path())) {
                        nextFilePath = match->filePath;
                        break;
                    }
                }
            }

            avalang::ui::LayoutRect viewport{0.0, 0.0, static_cast<double>(width),
                                              static_cast<double>(height)};
            avalang::ui::ILayoutNode* layoutRoot = layoutEngine->Compute(root, viewport);
            if (!layoutRoot) {
                Sleep(16);
                continue;
            }

            std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
                avalang::ui::render::IRenderTree::Create());
            renderTree->Build(root, layoutEngine.get());
            std::shared_ptr<avalang::ui::render::IRenderNode> renderRoot = renderTree->Root();
            if (!renderRoot) {
                Sleep(16);
                continue;
            }

            std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
                avalang::ui::scene::ISceneGraph::Create());
            sceneGraph->Build(renderRoot);
            sceneGraph->UpdateTransforms();
            if (!sceneGraph->Root()) {
                Sleep(16);
                continue;
            }

            avalang::ui::InteractiveState interactive;
            interactive.styles = &projectStyles;
            interactive.pointerX = dispatcher.PointerX();
            interactive.pointerY = dispatcher.PointerY();
            interactive.pointerDown =
                dispatcher.IsPointerButtonDown(avalang::ui::events::PointerButton::Left);
            interactive.focused = dispatcher.FocusedComponent();

            avalang::ui::RenderCommandSink sink;
            avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, renderer, &interactive);

            Sleep(16);
        }

        VmLifecycle::NotifyUnmount(host, stateBridge, root);

        if (surface->IsClosed()) {
            return 0;
        }
        if (!nextFilePath.empty()) {
            currentFilePath = nextFilePath;
            continue;
        }
        return 0;
    }

    return 0;
}

struct ApplicationContext {
    std::string projectDir;
    int width;
    int height;
    std::string lastError;
    int lastExitCode = 0;
};

namespace {

// __ava_native__.Run(viewPath) en vez de un __ava_application_run__(viewPath)
// suelto: Compiler::CheckCallArgs (compiler.cpp) valida en tiempo de
// compilacion que toda llamada NameExpr bare exista entre known_funcs_ del
// modulo / clases conocidas / AVA_BUILTIN_GLOBALS (builtin_names.h) -- y
// avanative registra este native ad-hoc via ava_vm_register_native() justo
// antes de compilar, asi que el compiler nunca lo ve y tira "function
// '__ava_application_run__' is not defined". Agregarlo a AVA_BUILTIN_GLOBALS
// no es correcto: esa lista es del engine avalang generico (builtin_init.cpp
// registra los C fn de ahi para *toda* VM), y ApplicationRunNative es
// especifico de avahost/avanative (necesita ApplicationContext). En cambio,
// Compiler::CheckMethodCallArgs (misma clase) solo valida `obj.metodo(...)`
// cuando el tipo estatico de `obj` es Type::Object con class_name conocida
// -- un dict dinamico no califica, asi que la llamada dotted se deja pasar
// sin chequeo estatico, igual que System.Console.WriteLine(...) (ver
// RegisterNativeModule/BuildNativeNamespace en builtins/system_module.cpp,
// mismo patron). Ver RunNativeApp() mas abajo para como se arma el dict.
const char* ApplicationClassSource() {
    return
        "class Application\n"
        "    func run(viewPath)\n"
        "        __ava_native__.Run(viewPath)\n"
        "    end\n"
        "end\n";
}

ava_value_t ApplicationRunNative(AvaVM* vm, const ava_value_t* args, size_t arg_count, void* user_data) {
    auto* ctx = static_cast<ApplicationContext*>(user_data);

    std::string viewPath;
    if (arg_count > 0 && args[0].type == AVA_STRING) {
        size_t len = 0;
        const char* data = ava_string_data(vm, args[0], &len);
        viewPath.assign(data, len);
    }

    if (viewPath.empty()) {
        ctx->lastError = "Application.run() requires a view path";
        ctx->lastExitCode = 1;
        return ava_value_t{};
    }

    std::filesystem::path resolved(viewPath);
    if (resolved.is_relative()) resolved = std::filesystem::path(ctx->projectDir) / resolved;

    ctx->lastExitCode = RunViewLoop(resolved.string(), ctx->width, ctx->height, ctx->lastError);
    return ava_value_t{};
}

}  // namespace

int RunNativeApp(const std::string& projectDir, const std::string& entryFile,
                  int width, int height, std::string& outError) {
    const std::filesystem::path entryPath = std::filesystem::path(projectDir) / entryFile;

    std::string entrySource;
    if (!ReadFile(entryPath.string(), entrySource, outError)) return 1;

    AvaVM* vm = ava_vm_create();
    if (!vm) {
        outError = "failed to create a VM to run '" + entryPath.string() + "'";
        return 1;
    }
    ava_vm_set_current_dir(vm, projectDir.c_str());
    ava_vm_add_search_path(vm, projectDir.c_str());

    ApplicationContext appCtx{projectDir, width, height};

    // Registrar como nombre bare (ava_vm_register_native) lo hace visible
    // solo en runtime (SetGlobal interno, ver VM::RegisterNative en
    // vm_core.cpp) -- el compiler no lo consulta. Se registra bajo un
    // nombre interno propio, se recupera como ava_value_t con
    // ava_get_global(), y se cuelga de un dict global ("__ava_native__")
    // para que ApplicationClassSource lo llame como __ava_native__.Run(...)
    // (dotted call, no bare call -- ver comentario en ApplicationClassSource
    // arriba sobre por que hace falta).
    ava_vm_register_native(vm, "__ava_application_run_impl__", ApplicationRunNative, &appCtx);
    ava_value_t runFn = ava_get_global(vm, "__ava_application_run_impl__");
    ava_value_t nativeNs = ava_dict_create(vm);
    ava_dict_set(vm, nativeNs, "Run", runFn);
    ava_set_global(vm, "__ava_native__", nativeNs);

    char* classError = nullptr;
    AvaModule* classModule = ava_compile(vm, ApplicationClassSource(), "<application>", &classError);
    if (!classModule) {
        outError = classError ? classError : "failed to define the Application class";
        if (classError) ava_string_free(classError);
        ava_vm_destroy(vm);
        return 1;
    }
    ava_value_t classResult{};
    char* classRunError = nullptr;
    ava_run(vm, classModule, &classResult, &classRunError);
    ava_module_destroy(classModule);
    if (classRunError) {
        outError = classRunError;
        ava_string_free(classRunError);
        ava_vm_destroy(vm);
        return 1;
    }

    char* compileError = nullptr;
    AvaModule* entryModule = ava_compile(vm, entrySource.c_str(), entryPath.string().c_str(), &compileError);
    if (!entryModule) {
        outError = compileError ? compileError : "failed to compile '" + entryPath.string() + "'";
        if (compileError) ava_string_free(compileError);
        ava_vm_destroy(vm);
        return 1;
    }

    ava_value_t entryResult{};
    char* entryRunError = nullptr;
    ava_run(vm, entryModule, &entryResult, &entryRunError);
    ava_module_destroy(entryModule);
    ava_vm_destroy(vm);

    if (entryRunError) {
        outError = entryRunError;
        ava_string_free(entryRunError);
        return 1;
    }

    if (appCtx.lastExitCode != 0 || !appCtx.lastError.empty()) {
        outError = appCtx.lastError;
        return appCtx.lastExitCode != 0 ? appCtx.lastExitCode : 1;
    }

    return 0;
}

}  // namespace native
}  // namespace avahost

#else

namespace avahost {
namespace native {

int RunNativeApp(const std::string&, const std::string&, int, int, std::string& outError) {
    outError = "native desktop hosting is only implemented for the Windows backend (Fase 6)";
    return 1;
}

}  // namespace native
}  // namespace avahost

#endif
