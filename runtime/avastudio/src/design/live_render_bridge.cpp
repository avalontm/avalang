#include "design/live_render_bridge.h"

#include <functional>

#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "parser/AvauiPropertyCoercion.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"

namespace studio::design {

LiveRenderResult BuildLiveRender(const DesignNode& root, int viewportWidth, int viewportHeight) {
    LiveRenderResult out;
    out.componentTree = avalang::ui::ComponentTree::Create();

    std::function<avalang::ui::IComponent*(const DesignNode&)> build =
        [&](const DesignNode& n) -> avalang::ui::IComponent* {
        using namespace avalang::ui;
        IComponent* comp = out.componentTree->CreateComponent(parser::CanonicalTypeName(n.type));
        out.uidToComponentId[n.node_uid] = comp->Id();

        if (!n.id.empty()) comp->SetProperty("id", PropertyValue(n.id));
        for (const auto& p : n.properties) {
            parser::SetPropertyWithAlias(comp, p.key, parser::InferValue(p.value));
        }
        for (const auto& ev : n.events) {
            comp->SetProperty(ev.key, PropertyValue(ev.value));
        }

        for (const auto& child : n.children) {
            comp->AddChild(build(child));
        }
        return comp;
    };
    out.componentTree->SetRoot(build(root));

    auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
        avalang::ui::CreateDefaultThemeProvider());
    if (!themeProvider) {
        out.error = "failed to create the default theme provider";
        return out;
    }
    avalang::ui::RenderTheme::Apply(out.componentTree.get(), themeProvider->Current());

    out.layoutEngine = avalang::ui::LayoutEngine::Create();
    avalang::ui::LayoutRect viewport{0.0, 0.0, static_cast<double>(viewportWidth),
                                      static_cast<double>(viewportHeight)};
    avalang::ui::ILayoutNode* layoutRoot =
        out.layoutEngine->Compute(out.componentTree->Root(), viewport);
    if (!layoutRoot) {
        out.error = "LayoutEngine::Compute failed";
        return out;
    }

    out.renderTree.reset(avalang::ui::render::IRenderTree::Create());
    out.renderTree->Build(out.componentTree->Root(), out.layoutEngine.get());
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

    std::unordered_map<avalang::ui::ComponentId, std::string> idToUid;
    for (auto& kv : out.uidToComponentId) idToUid[kv.second] = kv.first;
    std::function<void(avalang::ui::ILayoutNode*)> walk = [&](avalang::ui::ILayoutNode* ln) {
        if (!ln) return;
        auto it = idToUid.find(ln->Id());
        if (it != idToUid.end()) out.uidToRect[it->second] = ln->Rect();
        for (auto* c : ln->Children()) walk(c);
    };
    walk(layoutRoot);

    out.ok = true;
    return out;
}

} // namespace studio::design
