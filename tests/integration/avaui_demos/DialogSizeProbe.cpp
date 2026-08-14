// Minimal, standalone probe (not part of the normal build) to print the
// actual computed LayoutRect for every node in a Dialog's subtree, given
// a specific viewport size. State/click/VM bindings are bypassed on
// purpose (avalang's frontend is a stub in this sandbox) by hard-coding
// the same literal values ConfirmDialog.avaui's `state` block defaults
// to, so this exercises exactly the same view tree shape/properties the
// real app renders, just without the VM in the loop.
#include <cstdio>
#include <string>

#include "parser/AvauiParser.h"
#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "layout/LayoutEngine.h"
#include "layout/LayoutTypes.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "commands/SceneCommandWalker.h"
#include "commands/RenderCommandSink.h"
#include "renderer/HTMLRenderer.h"
#include "theme/RenderTheme.h"
#include "theme/ITheme.h"

using namespace avalang::ui;

namespace {

void PrintTree(IComponent* node, LayoutEngine* layout, int depth) {
    if (!node) return;
    ILayoutNode* ln = layout->FindNode(node->Id());
    std::string indent(static_cast<size_t>(depth) * 2, ' ');
    if (ln) {
        const LayoutRect& r = ln->Rect();
        std::printf("%s%-10s x=%-8.2f y=%-8.2f w=%-8.2f h=%-8.2f  (bottom=%.2f)\n",
                    indent.c_str(), node->TypeName().c_str(), r.x, r.y, r.width, r.height,
                    r.y + r.height);
    } else {
        std::printf("%s%-10s <no layout node>\n", indent.c_str(), node->TypeName().c_str());
    }
    for (IComponent* child : node->Children()) {
        PrintTree(child, layout, depth + 1);
    }
}

}  // namespace

int main() {
    // Same view shape as samples/web/testproj/components/ConfirmDialog.avaui,
    // with the state references replaced by their literal defaults and
    // isOpen forced to true.
    const std::string source = R"AVAUI(
view
    dialog
        id = "ConfirmDialog"
        title = "Eliminar elemento"
        isOpen = true
        dismissible = false
        width = 400

        column
            gap = 16
            padding = 24

            text
                text = "Eliminar elemento"
                fontSize = 18
                width = 352
                wrap = true
            end

            scrollview
                height = 96

                text
                    text = "Esta accion no se puede deshacer. Deseas continuar?"
                    fontSize = 14
                    textColor = "6B7280"
                    width = 352
                    wrap = true
                end
            end

            row
                gap = 12

                button
                    id = "ConfirmDialogCancelBtn"
                    text = "Cancelar"
                    backgroundColor = "F3F4F6"
                    textColor = "111827"
                end

                button
                    id = "ConfirmDialogAcceptBtn"
                    text = "Aceptar"
                    backgroundColor = "DC2626"
                end
            end
        end
    end
end
)AVAUI";

    parser::ParsedAvaui parsed = parser::AvauiParser::Parse(source);
    if (!parsed.tree) {
        std::printf("PARSE FAILED: no tree\n");
        return 1;
    }
    IComponent* root = parsed.tree->Root();
    if (!root) {
        std::printf("PARSE FAILED: no root\n");
        return 1;
    }

    // Must run before any layout/render-tree work -- this is what
    // injects type="dialog"'s theme defaults (overlay=true, backdrop=
    // true, surface background/border/radius), exactly like a real app
    // does via ui_pipeline_dynamic_renderer.cpp's RenderTreeFragment.
    // Skipping it (as an earlier version of this probe did) left every
    // Dialog's IsOverlay() false, silently invalidating any test of the
    // overlay-specific code paths (NearestScrollAncestor's Dialog
    // handling, the backdrop, etc.) without any error -- it just quietly
    // rendered the Dialog through the ordinary, non-overlay flow.
    auto themeProvider = std::unique_ptr<IThemeProvider>(CreateDefaultThemeProvider());
    RenderTheme::Apply(parsed.tree.get(), themeProvider->Current(), nullptr);

    auto layout = LayoutEngine::Create();

    // Try a few viewport sizes, including some short ones, to see
    // whether/when the button row's bottom edge exceeds the Dialog's
    // own bottom edge.
    const struct { double w, h; const char* label; } viewports[] = {
        {1280, 800, "1280x800 (typical desktop)"},
        {1280, 720, "1280x720"},
        {1024, 600, "1024x600 (short laptop)"},
        {800, 500, "800x500 (short window)"},
        {375, 667, "375x667 (mobile-ish)"},
    };

    for (const auto& vp : viewports) {
        std::printf("\n=== viewport %s ===\n", vp.label);
        layout->Compute(root, LayoutRect{0, 0, vp.w, vp.h});
        PrintTree(root, layout.get(), 0);
    }

    // Also render the full HTML for the first (typical desktop) viewport
    // and print it, to check the actual emitted CSS matches these rects
    // (not just the layout engine's own numbers).
    layout->Compute(root, LayoutRect{0, 0, 1280, 800});

    auto* renderTree = render::IRenderTree::Create();
    renderTree->Build(root, layout.get());

    auto* sceneGraph = scene::ISceneGraph::Create();
    sceneGraph->Build(renderTree->Root());

    HTMLRenderer htmlRenderer(1280, 800);
    htmlRenderer.SetFragmentOnly(false);
    RenderCommandSink sink;
    SceneCommandWalker::Walk(*sceneGraph, sink, htmlRenderer);

    std::printf("\n=== FULL HTML (1280x800) ===\n%s\n", htmlRenderer.GetOutput());

    return 0;
}
