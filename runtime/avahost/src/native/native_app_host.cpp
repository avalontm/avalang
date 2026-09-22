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
#include "components/PropertyValue.h"
#include "accessibility/AccessibilityTree.h"
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
#include "controls/ButtonController.h"
#include "controls/CheckBoxController.h"
#include "controls/RadioButtonController.h"
#include "controls/ComboBoxController.h"
#include "controls/ScrollViewController.h"
#include "controls/TextBoxEditingController.h"
#include "events/EventDispatcher.h"
#include "navigation/Navigator.h"
#include "platform/contract/IPlatform.h"
#include "platform/windows/GdiRenderer.h"
#include "platform/windows/WinAccessibilityBridge.h"
#include "platform/windows/WinKeyTranslation.h"
#include "platform/windows/WinMouse.h"
#include "perf/StartupProfiler.h"
#include "diagnostics/vm_debug.h"

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

// Collects every other RadioButton under `node` that shares `group`
// (`excludeId` is skipped). Mirrors CollectRadioButtonsInGroup from the web
// pipeline (ui_pipeline_dynamic_renderer.cpp) so both hosts deselect group
// siblings the same way.
void CollectRadioButtonsInGroup(avalang::ui::IComponent* node, const std::string& group,
                                 avalang::ui::ComponentId excludeId,
                                 std::vector<avalang::ui::IComponent*>& out) {
    if (!node) return;

    if (node->TypeName() == "RadioButton" && node->Id() != excludeId) {
        const avalang::ui::PropertyValue* groupProp = node->GetProperty("group");
        if (groupProp && groupProp->Type() == avalang::ui::PropertyType::String &&
            groupProp->AsString() == group) {
            out.push_back(node);
        }
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        CollectRadioButtonsInGroup(child, group, excludeId, out);
    }
}

// `agreed` yes; `a + b`, `"true"`, `x.y` no. Only a bare state variable can be
// written back by a control; anything else is an expression.
bool IsPlainIdentifier(const std::string& text) {
    if (text.empty()) return false;
    const unsigned char first = static_cast<unsigned char>(text.front());
    if (!(std::isalpha(first) || first == '_')) return false;
    for (char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || u == '_')) return false;
    }
    return true;
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

    // WinMouse::Position() reporta coordenadas de pantalla completa por
    // default; sin este SetWindow(), el HitTest/EventDispatcher (que
    // trabajan en coordenadas de cliente) quedan desfasados exactamente
    // por el offset de la ventana en el escritorio -- por eso "se ajusta"
    // al mover la ventana en vez de corregirse del todo.
    static_cast<avalang::ui::platform::windows::WinMouse&>(platform.Mouse()).SetWindow(hwnd);

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

        avalang::ui::platform::windows::WinAccessibility_SetDispatcher(&dispatcher);
        avalang::ui::platform::windows::WinAccessibility_SetComponentTree(parsed.tree.get());
        avalang::ui::platform::windows::WinAccessibility_SetLayoutEngine(layoutEngine.get());
        std::unique_ptr<avalang::ui::accessibility::AccessibilityTree> accessibilityTree;
        unsigned long long accessibilityTreeVersion = 0;
        double accessibilityTreeWidth = -1.0;
        double accessibilityTreeHeight = -1.0;
        avalang::ui::ComponentId accessibilityTreeFocus = 0;

        avalang::ui::controls::TextBoxEditingController textEditing(dispatcher, platform.Clipboard());
        textEditing.Attach(root);

        avalang::ui::controls::ButtonController buttonController(dispatcher);
        buttonController.Attach(root);

        avalang::ui::controls::CheckBoxController checkBoxController(dispatcher);
        checkBoxController.Attach(root);

        // Two-way binding for `isChecked = someStateVar`. Without this the
        // controller read the binding text as "unchecked", toggled from the
        // wrong value (first click did nothing visible) and then overwrote
        // the binding with a literal bool, so the state variable never
        // changed and the checkbox stopped following it.
        {
            avalang::ui::controls::CheckBoxBinding checkBoxBinding;
            checkBoxBinding.resolveChecked = [&stateBridge](avalang::ui::IComponent* checkBox) {
                const avalang::ui::PropertyValue* prop = checkBox->GetProperty("isChecked");
                if (!prop) return false;
                if (prop->Type() == avalang::ui::PropertyType::Bool) return prop->AsBool();
                // `isChecked = {agreed}` parses as Expression, not String (same
                // as any other `{binding}`); AsString() still returns the raw
                // "agreed" source for both, so both must be evaluated here.
                if (prop->Type() == avalang::ui::PropertyType::String ||
                    prop->Type() == avalang::ui::PropertyType::Expression) {
                    return stateBridge.EvalIdentifier(prop->AsString()) == "true";
                }
                return false;
            };
            checkBoxBinding.commitChecked = [&stateBridge](avalang::ui::IComponent* checkBox,
                                                            bool newChecked) {
                const avalang::ui::PropertyValue* prop = checkBox->GetProperty("isChecked");
                if (!prop || (prop->Type() != avalang::ui::PropertyType::String &&
                              prop->Type() != avalang::ui::PropertyType::Expression)) return false;
                const std::string variable = prop->AsString();
                if (!IsPlainIdentifier(variable)) return false;
                avalang::ui::IState* state = stateBridge.Find(variable);
                if (!state) return false;
                state->Set(avalang::ui::PropertyValue(newChecked));
                return true;
            };
            checkBoxController.SetBinding(std::move(checkBoxBinding));
        }

        avalang::ui::controls::RadioButtonController radioButtonController(dispatcher);
        radioButtonController.Attach(root);

        // Two-way binding for `selected = someStateVar`, mirroring the
        // CheckBox fix above. Without it RadioButtonController only ever
        // toggled the plain `isSelected` property via its internal
        // g_groups bookkeeping (RadioButton.cpp): the bound variable
        // (`planBasic` / `planPro`) never changed, and sibling RadioButtons
        // bound to their own variables never got deselected -- only their
        // plain property did, which nothing else reads once a binding is
        // in play.
        {
            avalang::ui::controls::RadioButtonBinding radioButtonBinding;
            radioButtonBinding.resolveSelected = [&stateBridge](avalang::ui::IComponent* radioButton) {
                const avalang::ui::PropertyValue* prop = radioButton->GetProperty("isSelected");
                if (!prop) return false;
                if (prop->Type() == avalang::ui::PropertyType::Bool) return prop->AsBool();
                if (prop->Type() == avalang::ui::PropertyType::String ||
                    prop->Type() == avalang::ui::PropertyType::Expression) {
                    return stateBridge.EvalIdentifier(prop->AsString()) == "true";
                }
                return false;
            };
            radioButtonBinding.commitSelected = [&stateBridge, &root](avalang::ui::IComponent* radioButton) {
                const avalang::ui::PropertyValue* prop = radioButton->GetProperty("isSelected");
                if (!prop || (prop->Type() != avalang::ui::PropertyType::String &&
                              prop->Type() != avalang::ui::PropertyType::Expression)) {
                    return false;
                }
                const std::string variable = prop->AsString();
                if (!IsPlainIdentifier(variable)) return false;
                avalang::ui::IState* state = stateBridge.Find(variable);
                if (!state) return false;

                const avalang::ui::PropertyValue* groupProp = radioButton->GetProperty("group");
                if (groupProp && groupProp->Type() == avalang::ui::PropertyType::String) {
                    std::vector<avalang::ui::IComponent*> siblings;
                    CollectRadioButtonsInGroup(root, groupProp->AsString(), radioButton->Id(), siblings);
                    for (avalang::ui::IComponent* sibling : siblings) {
                        const avalang::ui::PropertyValue* siblingProp = sibling->GetProperty("isSelected");
                        if (!siblingProp || (siblingProp->Type() != avalang::ui::PropertyType::String &&
                                              siblingProp->Type() != avalang::ui::PropertyType::Expression)) {
                            continue;
                        }
                        const std::string siblingVar = siblingProp->AsString();
                        if (!IsPlainIdentifier(siblingVar)) continue;
                        if (avalang::ui::IState* siblingState = stateBridge.Find(siblingVar)) {
                            siblingState->Set(avalang::ui::PropertyValue(false));
                        }
                    }
                }

                state->Set(avalang::ui::PropertyValue(true));
                return true;
            };
            radioButtonController.SetBinding(std::move(radioButtonBinding));
        }

        avalang::ui::controls::ComboBoxController comboBoxController(dispatcher);
        comboBoxController.Attach(root);

        avalang::ui::controls::ScrollViewController scrollViewController(dispatcher, *layoutEngine);
        scrollViewController.Attach(root);

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

            const unsigned long long rootVersion = root->Version();
            const avalang::ui::ComponentId focusedComponent = dispatcher.FocusedComponent();
            const bool accessibilityDirty = !accessibilityTree ||
                rootVersion != accessibilityTreeVersion ||
                viewport.width != accessibilityTreeWidth ||
                viewport.height != accessibilityTreeHeight ||
                focusedComponent != accessibilityTreeFocus;
            if (accessibilityDirty) {
                accessibilityTree = avalang::ui::accessibility::AccessibilityTree::Create(
                    parsed.tree.get(), layoutEngine.get(), focusedComponent);
                avalang::ui::platform::windows::WinAccessibility_SetTree(accessibilityTree.get());
                accessibilityTreeVersion = rootVersion;
                accessibilityTreeWidth = viewport.width;
                accessibilityTreeHeight = viewport.height;
                accessibilityTreeFocus = focusedComponent;
            }

            std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
                avalang::ui::render::IRenderTree::Create());
            // Sin esto, RenderTree::Eval() cae en su fallback (devuelve el
            // raw string tal cual) y en pantalla aparece literalmente el
            // binding sin evaluar, p.ej. `"test - clicks: " + clicks` en
            // vez del valor -- ver el mismo wiring en
            // ui_pipeline_dynamic_renderer.cpp (pipeline web), que si lo hace.
            renderTree->SetEvalText([&stateBridge](const std::string& raw) {
                return stateBridge.EvalIdentifier(raw);
            });
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

        avalang::ui::platform::windows::WinAccessibility_SetTree(nullptr);
        avalang::ui::platform::windows::WinAccessibility_SetDispatcher(nullptr);
        avalang::ui::platform::windows::WinAccessibility_SetComponentTree(nullptr);
        avalang::ui::platform::windows::WinAccessibility_SetLayoutEngine(nullptr);

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
    ava::diag::ApplyDebugMode(vm);
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
    const std::string entryStack = ava::diag::CaptureDebugStack(vm);
    ava_module_destroy(entryModule);
    ava_vm_destroy(vm);

    if (entryRunError) {
        outError = entryRunError;
        if (!entryStack.empty()) outError += "\nstack traceback:\n" + entryStack;
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
