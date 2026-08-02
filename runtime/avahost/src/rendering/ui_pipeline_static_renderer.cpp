#include "ui_pipeline_static_renderer.h"

#include <memory>
#include <stdexcept>

#include "components/ComponentTree.h"
#include "parser/AvauiParser.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"
#include "renderer/HTMLRenderer.h"

// Private headers of avalang_ui, same pattern
// tests/integration/avaui_demos/ButtonDemo.cpp already uses to reach outside the
// library target itself -- avalang_ui's public include dir is all of
// its own src/ (see runtime/avaui/CMakeLists.txt,
// `target_include_directories(avalang_ui PUBLIC .../src)`), so these
// resolve the same way for any consumer, not just avalang_ui's own
// demos.
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"

namespace avahost {

bool RenderAvauiStatic(const std::string& avauiSource, const UiPipelineRenderOptions& options,
                       std::string& outHtml, std::string& outError) {
    outHtml.clear();
    outError.clear();

    try {
        // Parser (Fase 14, extended Fase 19.4 with `animate`) -- throws
        // avalang::ui::parser::ParseError on malformed source, caught
        // below same as every other structural failure in this
        // pipeline.
        avalang::ui::parser::ParsedAvaui parsed = avalang::ui::parser::AvauiParser::Parse(avauiSource);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "parsed .avaui produced an empty component tree "
                       "(no top-level component inside 'view')";
            return false;
        }
        avalang::ui::IComponent* root = parsed.tree->Root();

        // Theme (Fase 16) -- default theme only; avalang.ui has no
        // notion of a project-configured theme yet, same gap
        // html_renderer.cpp's own styling (hardcoded CSS classes) has
        // on the existing path.
        auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
            avalang::ui::CreateDefaultThemeProvider());
        if (!themeProvider) {
            outError = "failed to create the default theme provider";
            return false;
        }
        avalang::ui::RenderTheme::Apply(parsed.tree.get(), themeProvider->Current());

        // Layout (Fase 3)
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

        // Render Tree (Fase 6)
        std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
            avalang::ui::render::IRenderTree::Create());
        renderTree->Build(root, layoutEngine.get());
        auto renderRoot = renderTree->Root();
        if (!renderRoot) {
            outError = "IRenderTree::Build produced an empty render tree";
            return false;
        }

        // Scene Graph (Fase 7)
        std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
            avalang::ui::scene::ISceneGraph::Create());
        sceneGraph->Build(renderRoot);
        sceneGraph->UpdateTransforms();
        if (!sceneGraph->Root()) {
            outError = "ISceneGraph::Build produced an empty scene";
            return false;
        }

        // Render Commands (Fase 8) + HTML Renderer (Fase 10)
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
        }
        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, *renderer);

        const char* html = renderer->GetOutput();
        outHtml = html ? html : "";
        return true;

    } catch (const std::exception& e) {
        outError = e.what();
        return false;
    }
}

} // namespace avahost
