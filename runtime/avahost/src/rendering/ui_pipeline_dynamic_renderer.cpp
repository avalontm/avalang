#include "ui_pipeline_dynamic_renderer.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "components/ComponentTree.h"
#include "composition/ComposePageWithLayout.h"
#include "parser/AvauiParser.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "theme/ProjectFontOverrides.h"
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
        // See ui_pipeline_static_renderer.cpp -- same app.ava
        // `font "role" "name" "path"` overlay, same passthrough-when-empty
        // ProjectTheme wrapper.
        avalang::ui::theme::ProjectTheme projectTheme(
            themeProvider->Current(),
            avalang::ui::theme::LoadProjectFontOverrides(options.projectRoot));
        avalang::ui::RenderTheme::Apply(tree, &projectTheme);

        substage = "RenderTreeFragment: layout engine compute";
        auto layoutEngine = avalang::ui::LayoutEngine::Create();
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

// Writes a TextBox/ComboBox's new client-side value into the VM state
// key it's bound to, before the pending handler runs -- otherwise the
// handler (and the re-render after it) still see the old value, since
// the client only ever POSTs {handler}, never the control's own value.
// `pendingCompId` is the id SceneCommandWalker now stamps onto the
// rendered <input>/<select> as data-comp-id; `text` (TextBox) /
// `selectedValue` (ComboBox) hold the raw bound identifier (e.g.
// `text = username`), same string VmStateBridge::EvalIdentifier reads
// on the way out. Only a bare identifier is supported -- anything else
// (a literal, an expression) isn't safely rewritable here and is left
// alone, same as if this function had never run.
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

void ResolveImportsAndMergeState(const std::string& projectRoot,
                                  const std::string& componentsDir,
                                  avalang::ui::parser::ParsedAvaui& parsed,
                                  std::unordered_map<std::string, std::string>& mergedState) {
    if (componentsDir.empty() || parsed.imports.empty()) return;
    if (!parsed.tree) return;

    mergedState = parsed.state;
    UiComponentResolver resolver(projectRoot, componentsDir);
    resolver.ResolveImports(parsed.tree.get(), parsed.imports, mergedState);
}

// Splits a rendered page fragment into `mainHtml` (everything else)
// and `overlayHtml` (every top-level `<div class="ava-overlay-fragment"
// style="...">` block SceneCommandWalker's pass 2 emitted, one per open
// overlay root -- e.g. a Dialog's backdrop plus its own box). Depth-aware
// rather than a plain substring search: an open Dialog's own children are
// themselves rendered as nested `<div>`s inside that marker, so the
// matching close has to be found by tracking nesting depth, not just
// the next literal "</div>".
//
// kMarker must stay byte-for-byte identical to the opening tag
// SceneCommandWalker.cpp's Pass 2 emits (including its inline
// `style="position:relative; z-index:2147483647;"`, added so this
// fragment also outranks a layout's trailing Footer -- see that file's
// comment on Pass 2) -- this is a plain substring search, not an HTML
// attribute parse, so any drift between the two silently stops this
// extraction from firing at all.
void ExtractOverlayFragments(const std::string& html, std::string& mainHtml, std::string& overlayHtml) {
    mainHtml.clear();
    overlayHtml.clear();
    static const std::string kMarker =
        "<div class=\"ava-overlay-fragment\" style=\"position:relative; z-index:2147483647;\">";

    size_t pos = 0;
    while (pos < html.size()) {
        size_t start = html.find(kMarker, pos);
        if (start == std::string::npos) {
            mainHtml += html.substr(pos);
            break;
        }
        mainHtml += html.substr(pos, start - pos);

        size_t scan = start + kMarker.size();
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
            // Unbalanced -- shouldn't happen given the marker always
            // self-closes, but keep the rest as main content rather
            // than silently drop it.
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
                        const UiPipelineRenderOptions& options, std::string& outHtml, std::string& outError) {
    std::string unusedStateJson;
    return RenderAvauiDynamicWithState(host, avauiSource, options, std::string(),
                                        std::string(), std::string(), std::string(),
                                        unusedStateJson, outHtml, outError);
}

bool RenderAvauiDynamicWithState(RuntimeHost& host, const std::string& avauiSource,
                                  const UiPipelineRenderOptions& options,
                                  const std::string& cachedStateJson,
                                  const std::string& pendingHandler,
                                  const std::string& pendingCompId,
                                  const std::string& pendingValue,
                                  std::string& outStateJson,
                                  std::string& outHtml, std::string& outError) {
    outHtml.clear();
    outError.clear();
    outStateJson.clear();

    try {
        avalang::ui::parser::ParsedAvaui parsed = avalang::ui::parser::AvauiParser::Parse(avauiSource);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree "
                       "(no top-level component inside 'view')";
            return false;
        }

        std::unordered_map<std::string, std::string> mergedState = parsed.state;
        ResolveImportsAndMergeState(options.projectRoot, options.componentsDir,
                                     parsed, mergedState);

        VmStateBridge stateBridge(host);
        stateBridge.BindWithOverlay(mergedState, cachedStateJson);

        host.BindCodeBehind(parsed.code);

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

        if (!RenderTreeFragment(parsed.tree.get(), options, /*fragmentOnly=*/false,
                                /*slotContent=*/std::string(), outHtml, outError,
                                &stateBridge)) {
            return false;
        }

        outStateJson = stateBridge.ExportJson();
        return true;

    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

bool RenderAvauiDynamicWithLayout(const std::string& projectRoot, RuntimeHost& host,
                                   const std::string& avauiSource,
                                   const UiPipelineRenderOptions& options,
                                   std::string& outHtml, std::string& outError) {
    std::string unusedStateJson;
    return RenderAvauiDynamicWithLayoutAndState(projectRoot, host, avauiSource, options,
                                                 std::string(), std::string(), std::string(),
                                                 std::string(), unusedStateJson,
                                                 outHtml, outError);
}

bool RenderAvauiDynamicWithLayoutAndState(const std::string& projectRoot, RuntimeHost& host,
                                           const std::string& avauiSource,
                                           const UiPipelineRenderOptions& options,
                                           const std::string& cachedStateJson,
                                           const std::string& pendingHandler,
                                           const std::string& pendingCompId,
                                           const std::string& pendingValue,
                                           std::string& outStateJson,
                                           std::string& outHtml, std::string& outError) {
    outHtml.clear();
    outError.clear();
    outStateJson.clear();

    // Tags whatever stage is currently running so that any exception
    // escaping it (including a bare std::bad_alloc, whose what() is
    // just "bad allocation" with zero context) gets prefixed with
    // *where* in the pipeline it happened -- previously the outer
    // catch alone left no way to tell "bind state" apart from "render
    // layout" from the client-facing 500 or the server log.
    std::string stage = "start";

    try {
        stage = "parse page";
        avalang::ui::parser::ParsedAvaui parsed = avalang::ui::parser::AvauiParser::Parse(avauiSource);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree "
                       "(no top-level component inside 'view')";
            return false;
        }

        // The layout (`extends "..."`) is read and parsed here, before
        // state is bound, so its own `import components.X` + `X()`
        // calls (e.g. a Navbar()/Footer() declared in layouts/main.avaui
        // itself, not in the page) go through the same import resolver
        // as the page. Previously only `parsed` (the page) was resolved
        // here -- the layout's tree was parsed and rendered further
        // below without ever calling ResolveImportsAndMergeState on it,
        // so any component imported *by the layout* rendered as an
        // empty <div></div> no matter how correct the page itself was.
        avalang::ui::parser::ParsedAvaui layoutParsed;
        bool haveLayout = false;
        if (!parsed.extends.empty()) {
            stage = "parse layout";
            fs::path layoutPath = avalang::ui::ResolveDottedAvauiPath(projectRoot, parsed.extends);
            std::string layoutSource;
            std::string readError;
            if (ReadFile(layoutPath, layoutSource, readError)) {
                try {
                    layoutParsed = avalang::ui::parser::AvauiParser::Parse(layoutSource);
                } catch (const avalang::ui::parser::ParseError& e) {
                    outError = std::string("layout parse error in ") + layoutPath.string() + ": " + e.what();
                    return false;
                }
                if (!layoutParsed.tree || !layoutParsed.tree->Root()) {
                    outError = "layout " + layoutPath.string() + " produced an empty component tree";
                    return false;
                }
                haveLayout = true;
            }
            // A missing/unreadable layout file falls back to rendering
            // just the page, same as before -- handled after render below.
        }

        stage = "resolve imports/state (page)";
        std::unordered_map<std::string, std::string> mergedState = parsed.state;
        ResolveImportsAndMergeState(projectRoot, options.componentsDir, parsed, mergedState);
        if (haveLayout) {
            stage = "resolve imports/state (layout)";
            std::unordered_map<std::string, std::string> layoutState;
            ResolveImportsAndMergeState(projectRoot, options.componentsDir, layoutParsed, layoutState);
            // Page state wins on key collisions -- same "first writer
            // wins" rule UiComponentResolver::MergeStateMap already
            // applies when a page and an imported component both
            // declare the same state key.
            for (const auto& [key, value] : layoutState) {
                mergedState.emplace(key, value);
            }
        }

        stage = "bind state (VmStateBridge::BindWithOverlay)";
        VmStateBridge stateBridge(host);
        stateBridge.BindWithOverlay(mergedState, cachedStateJson);

        stage = "bind code-behind";
        host.BindCodeBehind(parsed.code);

        avalang::ui::IComponent* root = parsed.tree->Root();

        stage = "wire event handlers (page)";
        std::unique_ptr<avalang::ui::events::IEventDispatcher> dispatcher(
            avalang::ui::events::IEventDispatcher::Create());
        WireVmEventHandlers(root, *dispatcher, host, stateBridge);
        if (haveLayout) {
            // A layout can have its own interactive elements (e.g. a
            // Navbar with a click handler) independent of the page's --
            // wire those too, onto the same dispatcher/VM/state bridge.
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

        // Find where the layout's `slot()` placeholder actually lands
        // before rendering the page: without this, the page has no way
        // to know it should render into a Navbar-to-Footer strip
        // instead of the full canvas.
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
        UiPipelineRenderOptions pageOptions = options;
        if (haveSlotRect) {
            // Render the page against the space the layout actually
            // gives it, not the full canvas -- otherwise its own
            // background/content always start at (0,0) and cover
            // whatever the layout placed above it (e.g. a Navbar).
            pageOptions.viewportWidth = static_cast<int>(slotRect.width);
            pageOptions.viewportHeight = static_cast<int>(slotRect.height);
        }
        if (!RenderTreeFragment(parsed.tree.get(), pageOptions, /*fragmentOnly=*/true,
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
            // Position the already-rendered page fragment at the
            // slot's actual coordinates within the layout instead of
            // splicing it in raw at (0,0). `overflow: hidden` matches
            // what every other sized container in this renderer does
            // (see HTMLRenderer's `.ava-viewport`) so oversized page
            // content clips instead of spilling past the Footer.
            //
            // An open Dialog's overlay (backdrop + its own box, see
            // SceneCommandWalker's "ava-overlay-fragment" marker) is
            // pulled out first and kept OUTSIDE this box: it needs to
            // reach the full #ava-viewport -- Navbar and Footer
            // included -- not just the Navbar-to-Footer strip the
            // slot clips ordinary page content to. Still offset by
            // the slot's own origin (same left/top as the box below)
            // so the Dialog's own position:absolute coordinates,
            // computed in the page's local coordinate space, land
            // where the page fragment expects them on screen.
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

        // Layout text/click bindings now resolve against the same
        // stateBridge as the page (previously `nullptr` here, so e.g.
        // `{siteName}` inside layouts/main.avaui always rendered as
        // literal text instead of evaluating against VM state).
        stage = "render layout";
        return RenderTreeFragment(layoutParsed.tree.get(), options, /*fragmentOnly=*/false,
                                   pageHtml, outHtml, outError, &stateBridge);

    } catch (const std::bad_alloc&) {
        // what() alone is just "bad allocation" -- always tag it with
        // the stage so this doesn't come back as an unactionable dead
        // end a second time.
        outError = std::string("bad allocation during stage [") + stage + "]";
        return false;
    } catch (const std::exception& e) {
        outError = std::string("[") + stage + "] " + e.what();
        return false;
    }
}

}  // namespace avahost