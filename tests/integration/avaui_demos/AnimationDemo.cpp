/**
 * Fase 19.5 -- Animation Demo & End-to-End Validation
 *
 * Pipeline exercised (same shape as ButtonDemo.cpp/ControlsDemo.cpp,
 * Fases 17-18, extended with Fase 19's animation stack):
 *
 *   fixtures/animation_demo.avaui
 *     -> AvauiParser (Fase 14, extended 19.4: `animate` block)
 *     -> ComponentTree -> RenderTheme::Apply (Fase 16)
 *     -> LayoutEngine (Fase 3) -> IRenderTree (Fase 6)
 *     -> ISceneGraph (Fase 7)
 *     -> AnimationController::Create(sceneGraph) (Fase 19.3)
 *     -> animation::WireAnimations(...) (Fase 19.4) subscribes the
 *        fixture's `trigger = "click"` to a real IEventDispatcher
 *        (Fase 5)
 *     -> synthetic Click event dispatched at the button's own
 *        ComponentId -> AnimationController::Play() actually starts
 *     -> AnimationController::Update(dt) called across several
 *        simulated frames -> SceneCommandWalker -> HTMLRenderer at
 *        three time points (start / mid / end), each saved to its own
 *        HTML file so opacity changing frame-to-frame is visible in
 *        the actual output, not just asserted in C++.
 *
 * Usage:
 *   ava_animation_demo [output_prefix]
 *
 * Exit code:
 *   0 = all tests passed
 *   1 = test failed
 */

#include "parser/AvauiParser.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"
#include "events/IEventDispatcher.h"
#include "animation/AnimationController.h"
#include "animation/AnimationBinding.h"

// Private headers for demo (same pattern as ButtonDemo.cpp/
// AvauiPipelineDemo.cpp).
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "events/Event.h" // MouseEvent -- synthetic click for this demo only

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class TestLogger {
public:
    void Log(const std::string& msg) {
        std::cout << "[AnimationDemo] " << msg << "\n";
    }
    void Error(const std::string& msg) {
        std::cerr << "[AnimationDemo] ERROR: " << msg << "\n";
    }
    void Assert(bool condition, const std::string& msg) {
        if (!condition) {
            Error(msg);
            throw std::runtime_error(msg);
        }
    }
};

TestLogger logger;

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open .avaui file: " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Finds the first component whose "id" property matches `id` --
// mirrors how AvauiPipelineDemo/ControlsDemo locate a specific node
// without a dedicated lookup-by-authored-id API (Fase 14 only stores
// it as a regular property, see AvauiParser.cpp's ParseComponent).
avalang::ui::IComponent* FindById(avalang::ui::IComponent* node, const std::string& id) {
    if (!node) return nullptr;
    auto* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && idProp->AsString() == id) {
        return node;
    }
    for (auto* child : node->Children()) {
        if (auto* found = FindById(child, id)) return found;
    }
    return nullptr;
}

// Renders the current SceneGraph state to HTML and saves it -- used
// three times (start/mid/end) to make the animation's frame-by-frame
// effect visible in the actual output files, not just in asserted
// float comparisons.
void RenderFrame(avalang::ui::scene::ISceneGraph& sceneGraph, const std::string& path,
                 const std::string& label) {
    auto renderer = avalang::ui::IRenderer::Create("html", 400, 300);
    avalang::ui::RenderCommandSink sink;
    avalang::ui::SceneCommandWalker::Walk(sceneGraph, sink, *renderer);

    const char* htmlOut = renderer->GetOutput();
    std::string html(htmlOut ? htmlOut : "");

    std::ofstream out(path);
    out << html;
    out.close();

    logger.Log("  saved " + label + " -> " + path + " (" + std::to_string(html.length()) + " bytes)");
}

} // namespace

int main(int argc, char** argv) {
    const std::string outputPrefix = (argc > 1) ? argv[1] : "animation_demo";

    try {
        logger.Log("=== Fase 19: Animation Demo ===");

        // ===== Test 1: Parse .avaui with an `animate` block =====
        logger.Log("Test 1: Parse fixture with animate block");
        std::string source = ReadFile("fixtures/animation_demo.avaui");
        avalang::ui::parser::ParsedAvaui parsed = avalang::ui::parser::AvauiParser::Parse(source);
        logger.Assert(parsed.tree && parsed.tree->Root(), "Parser produced a null tree/root");
        logger.Assert(parsed.animations.size() == 1,
                      "Expected exactly 1 AnimationSpec, got " +
                          std::to_string(parsed.animations.size()));
        logger.Log("  OK parsed 1 animate block (property=" + parsed.animations[0].property +
                   ", trigger=" + parsed.animations[0].trigger + ")");

        avalang::ui::IComponent* root = parsed.tree->Root();
        avalang::ui::IComponent* button = FindById(root, "AnimatedButton");
        logger.Assert(button, "Could not find 'AnimatedButton' in parsed tree");
        avalang::ui::ComponentId buttonId = button->Id();
        logger.Assert(parsed.animations[0].target == buttonId,
                      "AnimationSpec.target does not match the button's own ComponentId");
        logger.Log("  OK AnimationSpec targets the parsed button (id=" + std::to_string(buttonId) + ")");

        // ===== Test 2: Theme + Layout + RenderTree + SceneGraph =====
        logger.Log("Test 2: Run Theme/Layout/RenderTree/SceneGraph pipeline");
        auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
            avalang::ui::CreateDefaultThemeProvider());
        logger.Assert(themeProvider.get(), "Failed to create theme provider");
        avalang::ui::RenderTheme::Apply(parsed.tree.get(), themeProvider->Current());

        auto layoutEngine = avalang::ui::LayoutEngine::Create();
        avalang::ui::LayoutRect viewport{0.0, 0.0, 400.0, 300.0};
        auto* layoutRoot = layoutEngine->Compute(root, viewport);
        logger.Assert(layoutRoot, "LayoutEngine failed to compute layout");

        std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
            avalang::ui::render::IRenderTree::Create());
        renderTree->Build(root, layoutEngine.get());
        auto renderRoot = renderTree->Root();
        logger.Assert(renderRoot != nullptr, "Failed to build RenderTree");

        std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
            avalang::ui::scene::ISceneGraph::Create());
        sceneGraph->Build(renderRoot);
        sceneGraph->UpdateTransforms();
        logger.Assert(sceneGraph->Root() != nullptr, "Failed to build SceneGraph");
        logger.Log("  OK pipeline built (Theme -> Layout -> RenderTree -> SceneGraph)");

        auto sceneNode = sceneGraph->FindNode(buttonId);
        logger.Assert(sceneNode != nullptr, "SceneGraph has no node for the button's ComponentId");
        logger.Assert(std::fabs(sceneNode->Opacity() - 1.0f) < 0.001f,
                      "Button should start at full opacity before any animation plays");
        logger.Log("  OK initial opacity = " + std::to_string(sceneNode->Opacity()));

        // ===== Test 3: Wire the parsed animate block =====
        logger.Log("Test 3: AnimationController + WireAnimations (click trigger)");
        auto controller = avalang::ui::animation::AnimationController::Create(sceneGraph.get());
        logger.Assert(controller.get(), "Failed to create AnimationController");

        std::unique_ptr<avalang::ui::events::IEventDispatcher> dispatcher(
            avalang::ui::events::IEventDispatcher::Create());
        logger.Assert(dispatcher.get(), "Failed to create IEventDispatcher");

        // No `state`-driven triggers in this fixture -- empty map is
        // the correctly-typed "no named states available" case
        // documented in AnimationBinding.h.
        std::unordered_map<std::string, avalang::ui::IState*> states;
        avalang::ui::animation::WireAnimations(parsed.animations, controller.get(),
                                               dispatcher.get(), states);
        logger.Log("  OK WireAnimations subscribed the click trigger");

        // Render the "before click" frame -- opacity should still be 1.0.
        RenderFrame(*sceneGraph, outputPrefix + "_0_before.html", "before click");

        // ===== Test 4: Dispatch a real Click event -> animation starts =====
        logger.Log("Test 4: Dispatch synthetic Click at the button's ComponentId");
        avalang::ui::events::MouseEvent clickEvent(
            avalang::ui::events::EventType::Click, buttonId,
            avalang::ui::events::MouseButton::Left, 10, 10);
        dispatcher->Dispatch(clickEvent.AsIEvent());
        logger.Log("  OK Click dispatched -- AnimationController::Play() should have run");

        // Immediately after Play(), the controller applies the `from`
        // value (see AnimationControllerImpl::Play) -- opacity is
        // still 1.0 at t=0, but the animation is now active, so the
        // very next Update() call actually moves it.
        logger.Assert(std::fabs(sceneNode->Opacity() - 1.0f) < 0.001f,
                      "Opacity should still read 'from' (1.0) at t=0, right after Play()");
        logger.Log("  OK opacity at t=0 = " + std::to_string(sceneNode->Opacity()));

        // ===== Test 5: Update() loop -- simulate frames =====
        logger.Log("Test 5: AnimationController::Update() across simulated frames");
        const float dt = 0.1f; // 100ms/frame, matches the fixture's duration=0.4 (4 frames to finish)

        controller->Update(dt); // t=0.1
        sceneGraph->UpdateTransforms();
        float opacityAtMid1 = sceneNode->Opacity();
        logger.Assert(opacityAtMid1 < 1.0f && opacityAtMid1 > 0.2f,
                      "Opacity should be strictly between 'from' and 'to' mid-animation");
        logger.Log("  OK opacity at t=0.1 = " + std::to_string(opacityAtMid1));
        RenderFrame(*sceneGraph, outputPrefix + "_1_mid.html", "mid-animation (t=0.1)");

        controller->Update(dt); // t=0.2
        sceneGraph->UpdateTransforms();
        float opacityAtMid2 = sceneNode->Opacity();
        logger.Assert(opacityAtMid2 < opacityAtMid1,
                      "Opacity should keep decreasing frame over frame (fade out)");
        logger.Log("  OK opacity at t=0.2 = " + std::to_string(opacityAtMid2));

        controller->Update(dt); // t=0.3
        controller->Update(dt); // t=0.4 -- reaches the fixture's duration, PlaybackMode::Once stops here
        controller->Update(dt); // t=0.5 -- past duration; Once should hold the final value, not overshoot
        sceneGraph->UpdateTransforms();
        float opacityAtEnd = sceneNode->Opacity();
        logger.Assert(std::fabs(opacityAtEnd - 0.2f) < 0.01f,
                      "Opacity should have settled at 'to' (0.2) once the animation finished");
        logger.Log("  OK opacity at t>=0.4 (settled) = " + std::to_string(opacityAtEnd));
        RenderFrame(*sceneGraph, outputPrefix + "_2_end.html", "end of animation (t>=0.4)");

        logger.Log("");
        logger.Log("=== ALL TESTS PASSED ===");
        logger.Log("Phase 19 validation: PASS");

        return 0;

    } catch (const std::exception& e) {
        logger.Error("Test failed: " + std::string(e.what()));
        return 1;
    }
}
