#include "ui_pipeline_dynamic_renderer.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "components/ComponentTree.h"
#include "composition/ComposePageWithLayout.h"
#include "events/AutoBind.h"
#include "parser/AvauiParser.h"
#include "parser/AvauiPropertyCoercion.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "theme/ProjectFontOverrides.h"
#include "theme/ProjectStyleOverrides.h"
#include "theme/ProjectAnimationOverrides.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"
#include "renderer/HTMLRenderer.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "events/IEventDispatcher.h"

#include "resolver/DottedPath.h"
#include "runtime/runtime_host.h"
#include "ui_component_resolver.h"
#include "ui_pipeline_static_renderer.h"
#include "ui_vm_event_bridge.h"
#include "ui_vm_state_bridge.h"

namespace fs = std::filesystem;

namespace avahost {

class VmStateBridge;

namespace {

std::string ResolvePageTitle(const avalang::ui::parser::ParsedAvaui& pageParsed,
                              const avalang::ui::parser::ParsedAvaui* layoutParsed,
                              const std::string& fallbackTitle) {
    auto it = pageParsed.properties.find("title");
    if (it != pageParsed.properties.end() && !it->second.empty()) {
        return it->second;
    }
    if (layoutParsed) {
        auto layoutIt = layoutParsed->properties.find("title");
        if (layoutIt != layoutParsed->properties.end() && !layoutIt->second.empty()) {
            return layoutIt->second;
        }
    }
    return fallbackTitle;
}

bool ReadFile(const fs::path& path, std::string& out, std::string& outError) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        outError = "could not read " + path.string();
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    out = buf.str();
    return true;
}

bool RenderTreeFragment(avalang::ui::ComponentTree* tree,
                        const UiPipelineRenderOptions& options,
                        bool fragmentOnly,
                        std::string slotContent,
                        std::string& outHtml,
                        std::string& outError,
                        VmStateBridge* stateBridge) {
    avalang::ui::IComponent* root = tree->Root();
    if (!root) {
        outError = "RenderTreeFragment: empty component tree";
        return false;
    }

    const char* substage = "RenderTreeFragment: start";
    try {
        substage = "RenderTreeFragment: create theme provider";
        auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
            avalang::ui::CreateDefaultThemeProvider());
        if (!themeProvider) {
            outError = "failed to create the default theme provider";
            return false;
        }
        substage = "RenderTreeFragment: apply theme";

        avalang::ui::theme::ProjectTheme projectTheme(
            themeProvider->Current(),
            avalang::ui::theme::LoadProjectFontOverrides(options.projectRoot));
        projectTheme.RegisterProjectFonts();
        avalang::ui::theme::ProjectStyleSheet projectStyles =
            avalang::ui::theme::LoadProjectStyleOverrides(options.projectRoot);
        avalang::ui::RenderTheme::Apply(tree, &projectTheme, &projectStyles);

        avalang::ui::theme::ProjectAnimationSheet projectAnimations =
            avalang::ui::theme::LoadProjectAnimationOverrides(options.projectRoot);

        substage = "RenderTreeFragment: layout engine compute";
        auto layoutEngine = avalang::ui::LayoutEngine::Create();
        if (stateBridge) {

            layoutEngine->SetTextEvaluator([stateBridge](const std::string& raw) {
                return stateBridge->EvalIdentifier(raw);
            });
        }
        avalang::ui::LayoutRect viewport{
            0.0, 0.0,
            static_cast<double>(options.viewportWidth),
            static_cast<double>(options.viewportHeight)};
        avalang::ui::ILayoutNode* layoutRoot = layoutEngine->Compute(root, viewport);
        if (!layoutRoot) {
            outError = "LayoutEngine::Compute failed";
            return false;
        }

        substage = "RenderTreeFragment: build render tree";
        std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
            avalang::ui::render::IRenderTree::Create());
        if (stateBridge) {
            renderTree->SetEvalText([stateBridge](const std::string& raw) {
                return stateBridge->EvalIdentifier(raw);
            });
        }
        renderTree->Build(root, layoutEngine.get());
        auto renderRoot = renderTree->Root();
        if (!renderRoot) {
            outError = "IRenderTree::Build produced an empty render tree";
            return false;
        }

        substage = "RenderTreeFragment: build scene graph";
        std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
            avalang::ui::scene::ISceneGraph::Create());
        sceneGraph->Build(renderRoot);
        substage = "RenderTreeFragment: update transforms";
        sceneGraph->UpdateTransforms();
        if (!sceneGraph->Root()) {
            outError = "ISceneGraph::Build produced an empty scene";
            return false;
        }

        substage = "RenderTreeFragment: create HTML renderer";
        auto renderer = avalang::ui::IRenderer::Create("html", options.viewportWidth,
                                                        options.viewportHeight);
        if (!renderer) {
            outError = "IRenderer::Create(\"html\") failed";
            return false;
        }
        auto* htmlRenderer = dynamic_cast<avalang::ui::HTMLRenderer*>(renderer.get());
        if (htmlRenderer) {
            htmlRenderer->SetTitle(options.title);
            htmlRenderer->SetExtraHead(options.extraHead);
            htmlRenderer->SetExtraBodyEnd(options.extraBodyEnd);
            htmlRenderer->SetFragmentOnly(fragmentOnly);
            htmlRenderer->SetProjectStyles(&projectStyles);
            htmlRenderer->SetProjectAnimations(&projectAnimations);
            htmlRenderer->SetWwwRootDir(options.wwwrootDir);
        }
        substage = "RenderTreeFragment: walk scene commands";
        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, *renderer, slotContent);

        substage = "RenderTreeFragment: get renderer output";
        const char* html = renderer->GetOutput();
        outHtml = html ? html : "";
        return true;

    } catch (const std::bad_alloc&) {
        outError = std::string("bad allocation during [") + substage + "]";
        return false;
    }
}

 avalang::ui::IComponent* FindComponentById(avalang::ui::IComponent* root,
                                            avalang::ui::ComponentId id) {
    if (!root) return nullptr;
    if (root->Id() == id) return root;
    for (avalang::ui::IComponent* child : root->Children()) {
        if (avalang::ui::IComponent* found = FindComponentById(child, id)) {
            return found;
        }
    }
    return nullptr;
}

void ApplyPendingControlValue(VmStateBridge& stateBridge, avalang::ui::IComponent* root,
                               const std::string& pendingCompId, const std::string& pendingValue) {
    if (pendingCompId.empty()) return;

    avalang::ui::ComponentId id = 0;
    try {
        id = static_cast<avalang::ui::ComponentId>(std::stoull(pendingCompId));
    } catch (const std::exception&) {
        return;
    }

    avalang::ui::IComponent* comp = FindComponentById(root, id);
    if (!comp) return;

    const char* propName = nullptr;
    if (comp->TypeName() == "TextBox") {
        propName = "text";
    } else if (comp->TypeName() == "ComboBox") {
        propName = "selectedValue";
    } else {
        return;
    }

    const auto* prop = comp->GetProperty(propName);
    if (!prop || prop->Type() != avalang::ui::PropertyType::String) return;

    if (avalang::ui::IState* state = stateBridge.Find(prop->AsString())) {
        state->Set(avalang::ui::PropertyValue(pendingValue));
    }
}

void ResolveImportsAndMergeState(UiComponentResolver& resolver,
                                  avalang::ui::parser::ParsedAvaui& parsed,
                                  std::unordered_map<std::string, std::string>& mergedState) {
    if (parsed.imports.empty()) return;
    if (!parsed.tree) return;

    mergedState = parsed.state;
    resolver.ResolveImports(parsed.tree.get(), parsed.imports, mergedState, /*expandLoops=*/false);
}

avalang::ui::IComponent* CloneComponentSubtree(const avalang::ui::IComponent* src,
                                                avalang::ui::IComponent* parent,
                                                avalang::ui::ComponentTree* tree) {
    if (!src || !tree) return nullptr;
    avalang::ui::IComponent* clone = tree->CreateComponent(src->TypeName());
    if (!clone) return nullptr;

    for (const auto& name : src->PropertyNames()) {
        if (const auto* p = src->GetProperty(name)) {
            clone->SetProperty(name, *p);
        }
    }
    if (parent) parent->AddChild(clone);

    for (avalang::ui::IComponent* child : src->Children()) {
        CloneComponentSubtree(child, clone, tree);
    }
    return clone;
}

void BakeCallSiteExpressions(RuntimeHost& host, avalang::ui::IComponent* node) {
    if (!node) return;

    if (UiComponentResolver::IsComponentCall(node)) {
        for (const auto& propName : node->PropertyNames()) {
            if (propName == "id" || propName == "__unresolvedImportCall") continue;
            const auto* prop = node->GetProperty(propName);
            if (!prop || prop->Type() != avalang::ui::PropertyType::String) continue;

            bool ok = false;
            std::string literal = host.EvalExprToLiteral(prop->AsString(), ok);
            if (ok) {
                node->SetProperty(propName, avalang::ui::parser::InferValue(literal));
            }
        }
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        BakeCallSiteExpressions(host, child);
    }
}

std::vector<std::string> SplitHandlerArgs(const std::string& argsText) {
    std::vector<std::string> out;
    std::string current;
    int depth = 0;
    bool inString = false;
    bool inSingleString = false;
    for (size_t i = 0; i < argsText.size(); ++i) {
        char c = argsText[i];
        if (inString) {
            current += c;
            if (c == '\\' && i + 1 < argsText.size()) {
                current += argsText[++i];
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (inSingleString) {
            current += c;
            if (c == '\\' && i + 1 < argsText.size()) {
                current += argsText[++i];
            } else if (c == '\'') {
                inSingleString = false;
            }
            continue;
        }
        if (c == '"') { inString = true; current += c; continue; }
        if (c == '\'') { inSingleString = true; current += c; continue; }
        if (c == '(') { ++depth; current += c; continue; }
        if (c == ')') { --depth; current += c; continue; }
        if (c == ',' && depth == 0) {
            out.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (depth != 0 || inString || inSingleString) return {};
    out.push_back(current);
    return out;
}

bool IsAlreadyALiteral(const std::string& arg) {
    if (arg.empty()) return true;
    char first = arg.front();
    if (first == '"' || first == '\'' ) return true;
    if (first == '[' || first == '{') return true;
    if (std::isdigit(static_cast<unsigned char>(first))) return true;
    if (first == '-' && arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[1]))) return true;
    if (arg == "true" || arg == "false" || arg == "nil") return true;
    return false;
}

void BakeEventHandlerExpressions(RuntimeHost& host, avalang::ui::IComponent* node) {
    if (!node) return;

    for (const auto& propName : node->PropertyNames()) {
        if (!avalang::ui::IsEventPropertyName(propName)) continue;

        const auto* prop = node->GetProperty(propName);
        if (!prop || prop->Type() != avalang::ui::PropertyType::String) continue;

        const std::string& handler = prop->AsString();
        if (!avalang::ui::parser::LooksLikeCall(handler)) continue;

        size_t open = handler.find('(');
        size_t close = handler.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close <= open) continue;

        const std::string callee = handler.substr(0, open);
        const std::string argsText = handler.substr(open + 1, close - open - 1);

        std::vector<std::string> args = SplitHandlerArgs(argsText);
        if (args.empty() && !argsText.empty()) continue;

        std::ostringstream rebuilt;
        rebuilt << callee << "(";
        bool anyChanged = false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) rebuilt << ", ";
            std::string arg;
            {
                size_t a = args[i].find_first_not_of(" \t\r\n");
                size_t b = args[i].find_last_not_of(" \t\r\n");
                arg = (a == std::string::npos) ? std::string() : args[i].substr(a, b - a + 1);
            }
            if (arg.empty() || IsAlreadyALiteral(arg)) { rebuilt << arg; continue; }

            bool ok = false;
            std::string literal = host.EvalExprToLiteral(arg, ok);
            if (!ok) { rebuilt << arg; continue; }

            rebuilt << literal;
            anyChanged = true;
        }
        rebuilt << ")";

        if (anyChanged) {
            node->SetProperty(propName, avalang::ui::PropertyValue(rebuilt.str()));
        }
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        BakeEventHandlerExpressions(host, child);
    }
}

bool EvalConditionBool(RuntimeHost& host, const std::string& expr) {
    bool ok = false;
    std::string literal = host.EvalExprToLiteral(expr, ok);
    return ok && literal == "true";
}

int EvalListLength(RuntimeHost& host, const std::string& iterExpr) {
    bool ok = false;
    std::string literal = host.EvalExprToLiteral("len(" + iterExpr + ")", ok);
    if (!ok) return 0;
    try {
        return std::stoi(literal);
    } catch (const std::exception&) {
        return 0;
    }
}

void ExpandControlFlow(RuntimeHost& host, UiComponentResolver& resolver,
                        avalang::ui::ComponentTree* tree,
                        const std::vector<std::string>& imports,
                        std::unordered_map<std::string, std::string>& mergedState,
                        avalang::ui::IComponent* node) {
    if (!node || !tree) return;

    std::vector<avalang::ui::IComponent*> children = node->Children();
    for (avalang::ui::IComponent* child : children) {
        if (child->TypeName() == "If") {
            const auto* cond = child->GetProperty("condition");
            bool visible = cond && cond->Type() == avalang::ui::PropertyType::String &&
                           EvalConditionBool(host, cond->AsString());
            if (!visible) {
                for (avalang::ui::IComponent* grandchild : std::vector<avalang::ui::IComponent*>(child->Children())) {
                    child->RemoveChild(grandchild);
                }
                continue;
            }
            ExpandControlFlow(host, resolver, tree, imports, mergedState, child);
        } else if (child->TypeName() == "For") {
            const auto* loopVarProp = child->GetProperty("loopVar");
            const auto* iterProp = child->GetProperty("iterable");
            if (!loopVarProp || !iterProp) continue;

            std::string loopVar = loopVarProp->AsString();
            std::string iterExpr = iterProp->AsString();

            std::vector<avalang::ui::IComponent*> templateChildren = child->Children();
            for (avalang::ui::IComponent* templateChild : templateChildren) {
                child->RemoveChild(templateChild);
            }

            int count = EvalListLength(host, iterExpr);
            for (int i = 0; i < count; ++i) {
                host.EvalAssignGlobal(loopVar, iterExpr + "[" + std::to_string(i) + "]");
                host.EvalAssignGlobal("index", std::to_string(i));

                for (avalang::ui::IComponent* templateChild : templateChildren) {
                    avalang::ui::IComponent* clone = CloneComponentSubtree(templateChild, child, tree);
                    if (!clone) continue;
                    BakeCallSiteExpressions(host, clone);

                    if (UiComponentResolver::IsComponentCall(clone)) {
                        std::vector<avalang::ui::IComponent*> resolved =
                            resolver.ResolveCallSite(clone, tree, imports, mergedState);
                        child->RemoveChild(clone);
                        for (avalang::ui::IComponent* r : resolved) {
                            child->AddChild(r);
                        }
                    }
                }
            }
            ExpandControlFlow(host, resolver, tree, imports, mergedState, child);
        } else if (child->TypeName() == "ListView") {
            const auto* sourceProp = child->GetProperty("source");
            if (!sourceProp || sourceProp->Type() != avalang::ui::PropertyType::String) {
                ExpandControlFlow(host, resolver, tree, imports, mergedState, child);
                continue;
            }
            std::string iterExpr = sourceProp->AsString();

            std::string loopVar = "item";
            if (const auto* asProp = child->GetProperty("as")) {
                if (asProp->Type() == avalang::ui::PropertyType::String && !asProp->AsString().empty()) {
                    loopVar = asProp->AsString();
                }
            }

            std::vector<avalang::ui::IComponent*> templateChildren = child->Children();
            for (avalang::ui::IComponent* templateChild : templateChildren) {
                child->RemoveChild(templateChild);
            }

            int count = EvalListLength(host, iterExpr);
            for (int i = 0; i < count; ++i) {
                host.EvalAssignGlobal(loopVar, iterExpr + "[" + std::to_string(i) + "]");
                host.EvalAssignGlobal("index", std::to_string(i));

                for (avalang::ui::IComponent* templateChild : templateChildren) {
                    avalang::ui::IComponent* clone = CloneComponentSubtree(templateChild, child, tree);
                    if (!clone) continue;
                    BakeCallSiteExpressions(host, clone);

                    if (UiComponentResolver::IsComponentCall(clone)) {
                        std::vector<avalang::ui::IComponent*> resolved =
                            resolver.ResolveCallSite(clone, tree, imports, mergedState);
                        child->RemoveChild(clone);
                        for (avalang::ui::IComponent* r : resolved) {
                            child->AddChild(r);
                        }
                    }
                }
            }
            ExpandControlFlow(host, resolver, tree, imports, mergedState, child);
        } else {
            ExpandControlFlow(host, resolver, tree, imports, mergedState, child);
        }
    }
}

void ExtractOverlayFragments(const std::string& html, std::string& mainHtml, std::string& overlayHtml) {
    mainHtml.clear();
    overlayHtml.clear();
    static const std::string kMarkerPrefix =
        "<div class=\"ava-overlay-fragment\" data-dialog-id=\"";
    static const std::string kMarkerSuffix =
        "\" style=\"position:relative; z-index:2147483647;\">";

    size_t pos = 0;
    while (pos < html.size()) {
        size_t start = html.find(kMarkerPrefix, pos);
        if (start == std::string::npos) {
            mainHtml += html.substr(pos);
            break;
        }
        size_t suffixPos = html.find(kMarkerSuffix, start + kMarkerPrefix.size());
        if (suffixPos == std::string::npos) {
            mainHtml += html.substr(pos);
            break;
        }
        mainHtml += html.substr(pos, start - pos);

        size_t scan = suffixPos + kMarkerSuffix.size();
        int depth = 1;
        size_t closeAt = std::string::npos;
        while (scan < html.size()) {
            size_t nextOpen = html.find("<div", scan);
            size_t nextClose = html.find("</div>", scan);
            if (nextClose == std::string::npos) break;
            if (nextOpen != std::string::npos && nextOpen < nextClose) {
                ++depth;
                scan = nextOpen + 4;
            } else {
                --depth;
                scan = nextClose + 6;
                if (depth == 0) {
                    closeAt = scan;
                    break;
                }
            }
        }
        if (closeAt == std::string::npos) {
            mainHtml += html.substr(start);
            pos = html.size();
            break;
        }
        overlayHtml += html.substr(start, closeAt - start);
        pos = closeAt;
    }
}

} // namespace

bool RenderAvauiDynamic(RuntimeHost& host, const std::string& avauiSource,
                        const UiPipelineRenderOptions& options, std::string& outHtml, std::string& outError,
                        const std::string& avauiPath,
                        avalang::ui::parser::ParseErrorInfo* outParseError) {
    std::string unusedStateJson;
    return RenderAvauiDynamicWithState(host, avauiSource, options, std::string(),
                                        std::string(), std::string(), std::string(),
                                        unusedStateJson, outHtml, outError, avauiPath,
                                        outParseError);
}

bool RenderAvauiDynamicWithState(RuntimeHost& host, const std::string& avauiSource,
                                  const UiPipelineRenderOptions& options,
                                  const std::string& cachedStateJson,
                                  const std::string& pendingHandler,
                                  const std::string& pendingCompId,
                                  const std::string& pendingValue,
                                  std::string& outStateJson,
                                  std::string& outHtml, std::string& outError,
                                  const std::string& avauiPath,
                                  avalang::ui::parser::ParseErrorInfo* outParseError) {
    outHtml.clear();
    outError.clear();
    outStateJson.clear();

    try {
        avalang::ui::parser::ParsedAvaui parsed =
            avalang::ui::parser::AvauiParser::Parse(avauiSource, avauiPath);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree "
                       "(no top-level component inside 'view')";
            return false;
        }

        std::unordered_map<std::string, std::string> mergedState = parsed.state;
        UiComponentResolver resolver(options.projectRoot, options.componentsDir);
        ResolveImportsAndMergeState(resolver, parsed, mergedState);

        VmStateBridge stateBridge(host);
        stateBridge.BindWithOverlay(mergedState, cachedStateJson);

        std::string codeError;
        if (!host.BindCodeBehind(parsed.code, &codeError)) {
            outError = "code block failed to bind: " + codeError;
            return false;
        }

        avalang::ui::IComponent* root = parsed.tree->Root();

        std::unique_ptr<avalang::ui::events::IEventDispatcher> dispatcher(
            avalang::ui::events::IEventDispatcher::Create());
        WireVmEventHandlers(root, *dispatcher, host, stateBridge);

        std::string handlerError;
        BindComponentRefs(host, root);
        if (!host.InvokeHandlerIfDefined("OnLoad", handlerError)) {
            outError = "OnLoad handler failed: " + handlerError;
            return false;
        }
        ExportComponentProps(host, root);
        stateBridge.RefreshAll();

        if (!pendingHandler.empty()) {
            ApplyPendingControlValue(stateBridge, root, pendingCompId, pendingValue);
            BindComponentRefs(host, root);
            if (!host.InvokeHandler(pendingHandler, handlerError)) {
                outError = "event handler '" + pendingHandler + "' failed: " + handlerError;
                return false;
            }
            ExportComponentProps(host, root);
            stateBridge.RefreshAll();
        }

        ExpandControlFlow(host, resolver, parsed.tree.get(), parsed.imports, mergedState, root);
        BakeEventHandlerExpressions(host, root);

        UiPipelineRenderOptions pageOptions = options;
        pageOptions.title = ResolvePageTitle(parsed, /*layoutParsed=*/nullptr, options.title);

        if (!RenderTreeFragment(parsed.tree.get(), pageOptions, /*fragmentOnly=*/false,
                                /*slotContent=*/std::string(), outHtml, outError,
                                &stateBridge)) {
            return false;
        }

        outStateJson = stateBridge.ExportJson();
        return true;

    } catch (const avalang::ui::parser::ParseError& e) {
        outError = e.what();
        if (outParseError) {
            outParseError->message = e.RawMessage();
            outParseError->line = e.Line();
            outParseError->column = e.Column();
            outParseError->source = e.Source();
        }
        return false;
    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

bool RenderAvauiDynamicWithLayout(const std::string& projectRoot, RuntimeHost& host,
                                   const std::string& avauiSource,
                                   const UiPipelineRenderOptions& options,
                                   std::string& outHtml, std::string& outError,
                                   const std::string& avauiPath,
                                   avalang::ui::parser::ParseErrorInfo* outParseError) {
    std::string unusedStateJson;
    return RenderAvauiDynamicWithLayoutAndState(projectRoot, host, avauiSource, options,
                                                 std::string(), std::string(), std::string(),
                                                 std::string(), unusedStateJson,
                                                 outHtml, outError, avauiPath, outParseError);
}

bool RenderAvauiDynamicWithLayoutAndState(const std::string& projectRoot, RuntimeHost& host,
                                           const std::string& avauiSource,
                                           const UiPipelineRenderOptions& options,
                                           const std::string& cachedStateJson,
                                           const std::string& pendingHandler,
                                           const std::string& pendingCompId,
                                           const std::string& pendingValue,
                                           std::string& outStateJson,
                                           std::string& outHtml, std::string& outError,
                                           const std::string& avauiPath,
                                           avalang::ui::parser::ParseErrorInfo* outParseError) {
    outHtml.clear();
    outError.clear();
    outStateJson.clear();

    std::string stage = "start";

    try {
        stage = "parse page";
        avalang::ui::parser::ParsedAvaui parsed =
            avalang::ui::parser::AvauiParser::Parse(avauiSource, avauiPath);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree "
                       "(no top-level component inside 'view')";
            return false;
        }

        avalang::ui::parser::ParsedAvaui layoutParsed;
        bool haveLayout = false;
        if (!parsed.extends.empty()) {
            stage = "parse layout";
            fs::path layoutPath = avalang::ui::ResolveDottedAvauiPath(projectRoot, parsed.extends);
            std::string layoutSource;
            std::string readError;
            if (ReadFile(layoutPath, layoutSource, readError)) {
                try {
                    layoutParsed =
                        avalang::ui::parser::AvauiParser::Parse(layoutSource, layoutPath.string());
                } catch (const avalang::ui::parser::ParseError& e) {
                    outError = std::string("layout parse error in ") + layoutPath.string() + ": " + e.what();
                    if (outParseError) {
                        outParseError->message = e.RawMessage();
                        outParseError->line = e.Line();
                        outParseError->column = e.Column();
                        outParseError->source = e.Source();
                    }
                    return false;
                }
                if (!layoutParsed.tree || !layoutParsed.tree->Root()) {
                    outError = "layout " + layoutPath.string() + " produced an empty component tree";
                    return false;
                }
                haveLayout = true;
            }
        }

        stage = "resolve imports/state (page)";
        UiComponentResolver resolver(projectRoot, options.componentsDir);
        std::unordered_map<std::string, std::string> mergedState = parsed.state;
        ResolveImportsAndMergeState(resolver, parsed, mergedState);
        if (haveLayout) {
            stage = "resolve imports/state (layout)";
            std::unordered_map<std::string, std::string> layoutState;
            ResolveImportsAndMergeState(resolver, layoutParsed, layoutState);

            for (const auto& [key, value] : layoutState) {
                mergedState.emplace(key, value);
            }
        }

        stage = "bind state (VmStateBridge::BindWithOverlay)";
        VmStateBridge stateBridge(host);
        stateBridge.BindWithOverlay(mergedState, cachedStateJson);

        stage = "bind code-behind";
        std::string codeError;
        if (!host.BindCodeBehind(parsed.code, &codeError)) {
            outError = "code block failed to bind: " + codeError;
            return false;
        }

        avalang::ui::IComponent* root = parsed.tree->Root();

        stage = "wire event handlers (page)";
        std::unique_ptr<avalang::ui::events::IEventDispatcher> dispatcher(
            avalang::ui::events::IEventDispatcher::Create());
        WireVmEventHandlers(root, *dispatcher, host, stateBridge);
        if (haveLayout) {
            stage = "wire event handlers (layout)";
            WireVmEventHandlers(layoutParsed.tree->Root(), *dispatcher, host, stateBridge);
        }

        std::string handlerError;
        stage = "bind component refs (pre-OnLoad, page)";
        BindComponentRefs(host, root);
        if (haveLayout) {
            stage = "bind component refs (pre-OnLoad, layout)";
            BindComponentRefs(host, layoutParsed.tree->Root());
        }
        stage = "invoke OnLoad";
        if (!host.InvokeHandlerIfDefined("OnLoad", handlerError)) {
            outError = "OnLoad handler failed: " + handlerError;
            return false;
        }
        stage = "export component props (post-OnLoad, page)";
        ExportComponentProps(host, root);
        if (haveLayout) {
            stage = "export component props (post-OnLoad, layout)";
            ExportComponentProps(host, layoutParsed.tree->Root());
        }
        stage = "refresh state (post-OnLoad)";
        stateBridge.RefreshAll();

        if (!pendingHandler.empty()) {
            stage = "apply pending control value";
            ApplyPendingControlValue(stateBridge, root, pendingCompId, pendingValue);
            if (haveLayout) {
                ApplyPendingControlValue(stateBridge, layoutParsed.tree->Root(), pendingCompId, pendingValue);
            }
            stage = "bind component refs (pre-handler, page)";
            BindComponentRefs(host, root);
            if (haveLayout) {
                stage = "bind component refs (pre-handler, layout)";
                BindComponentRefs(host, layoutParsed.tree->Root());
            }
            stage = "invoke pending handler '" + pendingHandler + "'";
            if (!host.InvokeHandler(pendingHandler, handlerError)) {
                outError = "event handler '" + pendingHandler + "' failed: " + handlerError;
                return false;
            }
            stage = "export component props (post-handler, page)";
            ExportComponentProps(host, root);
            if (haveLayout) {
                stage = "export component props (post-handler, layout)";
                ExportComponentProps(host, layoutParsed.tree->Root());
            }
            stage = "refresh state (post-handler)";
            stateBridge.RefreshAll();
        }

        stage = "expand control flow (page)";
        ExpandControlFlow(host, resolver, parsed.tree.get(), parsed.imports, mergedState, root);
        if (haveLayout) {
            stage = "expand control flow (layout)";
            ExpandControlFlow(host, resolver, layoutParsed.tree.get(), layoutParsed.imports,
                               mergedState, layoutParsed.tree->Root());
        }

        stage = "bake event handler expressions (page)";
        BakeEventHandlerExpressions(host, root);
        if (haveLayout) {
            stage = "bake event handler expressions (layout)";
            BakeEventHandlerExpressions(host, layoutParsed.tree->Root());
        }

        avalang::ui::LayoutRect slotRect{0.0, 0.0,
                                         static_cast<double>(options.viewportWidth),
                                         static_cast<double>(options.viewportHeight)};
        bool haveSlotRect = false;
        if (haveLayout) {
            stage = "locate layout slot";
            auto slotInfo = avalang::ui::LocateLayoutSlot(layoutParsed.tree.get(),
                                                          options.viewportWidth,
                                                          options.viewportHeight);
            if (slotInfo.hasSlot) {
                slotRect = slotInfo.slotRect;
                haveSlotRect = true;
            }
        }

        stage = "render page fragment";
        std::string pageHtml;
        const std::string resolvedTitle =
            ResolvePageTitle(parsed, haveLayout ? &layoutParsed : nullptr, options.title);

        UiPipelineRenderOptions pageOptions = options;
        pageOptions.title = resolvedTitle;
        if (haveSlotRect) {
            pageOptions.viewportWidth = static_cast<int>(slotRect.width);
            pageOptions.viewportHeight = static_cast<int>(slotRect.height);
        }

        if (!RenderTreeFragment(parsed.tree.get(), pageOptions, /*fragmentOnly=*/haveLayout,
                                /*slotContent=*/std::string(), pageHtml, outError,
                                &stateBridge)) {
            return false;
        }

        stage = "export state json";
        outStateJson = stateBridge.ExportJson();

        if (!haveLayout) {
            outHtml = pageHtml;
            return true;
        }

        if (haveSlotRect) {
            std::string pageMainHtml, pageOverlayHtml;
            ExtractOverlayFragments(pageHtml, pageMainHtml, pageOverlayHtml);

            std::ostringstream wrapped;
            wrapped << "<div class=\"ava-element\" style=\"position: absolute; left: "
                    << slotRect.x << "px; top: " << slotRect.y << "px; width: "
                    << slotRect.width << "px; height: " << slotRect.height
                    << "px; overflow: hidden;\">" << pageMainHtml << "</div>";
            if (!pageOverlayHtml.empty()) {
                wrapped << "<div class=\"ava-element\" style=\"position: absolute; left: "
                        << slotRect.x << "px; top: " << slotRect.y << "px; width: 0; height: 0;\">"
                        << pageOverlayHtml << "</div>";
            }
            pageHtml = wrapped.str();
        }

        stage = "render layout";
        UiPipelineRenderOptions layoutOptions = options;
        layoutOptions.title = resolvedTitle;
        return RenderTreeFragment(layoutParsed.tree.get(), layoutOptions, /*fragmentOnly=*/false,
                                   pageHtml, outHtml, outError, &stateBridge);

    } catch (const std::bad_alloc&) {
        outError = std::string("bad allocation during stage [") + stage + "]";
        return false;
    } catch (const avalang::ui::parser::ParseError& e) {
        outError = std::string("[") + stage + "] " + e.what();
        if (outParseError) {
            outParseError->message = e.RawMessage();
            outParseError->line = e.Line();
            outParseError->column = e.Column();
            outParseError->source = e.Source();
        }
        return false;
    } catch (const std::exception& e) {
        outError = std::string("[") + stage + "] " + e.what();
        return false;
    }
}

}  // namespace avahost