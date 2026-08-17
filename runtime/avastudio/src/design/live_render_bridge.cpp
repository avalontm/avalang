#include "design/live_render_bridge.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "components/IComponent.h"
#include "composition/ComposePageWithLayout.h"
#include "parser/AvauiParser.h"
#include "resolver/DottedPath.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "theme/ProjectFontOverrides.h"
#include "theme/ProjectStyleOverrides.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"

namespace studio::design {

namespace {

std::string ReadFileToString(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::filesystem::path ResolveDottedPath(const std::string& projectRoot, const std::string& dotted) {
    return avalang::ui::ResolveDottedAvauiPath(projectRoot, dotted);
}

}

LiveRenderResult BuildLiveRender(avalang::ui::ComponentTree* tree, int viewportWidth, int viewportHeight,
                                  const std::string& extends,
                                  const std::string& projectRoot) {
    LiveRenderResult out;

    if (!tree || !tree->Root()) {
        out.error = "empty component tree";
        return out;
    }

    if (!extends.empty() && !projectRoot.empty()) {
        auto layoutPath = ResolveDottedPath(projectRoot, extends);
        std::string layoutSource = ReadFileToString(layoutPath.string());
        if (!layoutSource.empty()) {
            try {
                // Fase 2: pass the layout's own resolved path so a syntax
                // error inside it (found via this project's `extends`)
                // reports that file instead of an unlabeled line number.
                auto layoutParsed =
                    avalang::ui::parser::AvauiParser::Parse(layoutSource, layoutPath.string());
                if (layoutParsed.tree && layoutParsed.tree->Root()) {
                    auto slotInfo = avalang::ui::LocateLayoutSlot(layoutParsed.tree.get(),
                                                                  viewportWidth, viewportHeight);
                    out.layoutTree = std::move(layoutParsed.tree);
                    out.slotRect = slotInfo.slotRect;
                    out.hasSlot = slotInfo.hasSlot;
                }
            } catch (const std::exception&) {
            }
        }
    }

    auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
        avalang::ui::CreateDefaultThemeProvider());
    if (!themeProvider) {
        out.error = "failed to create the default theme provider";
        return out;
    }







    avalang::ui::theme::ProjectTheme projectTheme(
        themeProvider->Current(),
        avalang::ui::theme::LoadProjectFontOverrides(projectRoot));
    // See ui_pipeline_static_renderer.cpp / ProjectTheme::RegisterProjectFonts --
    // must run before RenderTheme::Apply so AvaStudio's live preview matches
    // what AvaHost actually ships (a component with an explicit fontName
    // never triggers RenderTheme's own lazy registration).
    projectTheme.RegisterProjectFonts();
    // See ui_pipeline_static_renderer.cpp -- same declared-style-file
    // overlay (`style *` / `style <type>` blocks), so AvaStudio's live
    // preview matches what AvaHost ships here too.
    avalang::ui::theme::ProjectStyleSheet projectStyles =
        avalang::ui::theme::LoadProjectStyleOverrides(projectRoot);
    avalang::ui::RenderTheme::Apply(tree, &projectTheme, &projectStyles);

    out.layoutEngine = avalang::ui::LayoutEngine::Create();
    int effectiveW = viewportWidth;
    int effectiveH = viewportHeight;
    if (out.hasSlot) {
        effectiveW = static_cast<int>(out.slotRect.width);
        effectiveH = static_cast<int>(out.slotRect.height);
    }
    avalang::ui::LayoutRect viewport{0.0, 0.0, static_cast<double>(effectiveW),
                                      static_cast<double>(effectiveH)};
    avalang::ui::ILayoutNode* layoutRoot =
        out.layoutEngine->Compute(tree->Root(), viewport);
    if (!layoutRoot) {
        out.error = "LayoutEngine::Compute failed";
        return out;
    }

    out.renderTree.reset(avalang::ui::render::IRenderTree::Create());
    out.renderTree->Build(tree->Root(), out.layoutEngine.get());
    if (!out.renderTree->Root()) {
        out.error = "IRenderTree::Build failed";
        return out;
    }

    out.sceneGraph.reset(avalang::ui::scene::ISceneGraph::Create());
    out.sceneGraph->Build(out.renderTree->Root());
    out.sceneGraph->UpdateTransforms();
    if (!out.sceneGraph->Root()) {
        out.error = "ISceneGraph::Build produced an empty scene";
        return out;
    }

    std::function<void(avalang::ui::ILayoutNode*)> walk = [&](avalang::ui::ILayoutNode* ln) {
        if (!ln) return;
        auto* comp = tree->FindById(ln->Id());
        if (comp) out.nodeIdToRect[comp->NodeId()] = ln->Rect();
        for (auto* c : ln->Children()) walk(c);
    };
    walk(layoutRoot);

    out.ok = true;
    return out;
}

}