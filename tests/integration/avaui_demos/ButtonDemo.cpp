/**
 * Fase 17 -- Button Control Demo & Validation
 *
 * Tests:
 * 1. Create Button via ComponentTree
 * 2. Apply Theme (validate Theme + Button integration)
 * 3. Bind click callback
 * 4. Process through full pipeline: Layout -> RenderTree -> SceneGraph -> HTML Renderer
 * 5. Verify Button renders as Rectangle + Text with proper styling
 *
 * NOTE (found during Fase 18 testing): this file previously called a set
 * of free functions (avalang::ui::CreateComponentTree/CreateLayoutEngine/
 * CreateRenderTree/CreateSceneGraph/CreateHTMLRenderer, and an
 * IRenderer::Render(sceneGraph, ostream) overload) that do not exist
 * anywhere in the codebase -- it never actually compiled. The real
 * factories are static Create() methods on each interface, same pattern
 * ui/tests/AvauiPipelineDemo.cpp (Fase 14) already uses correctly. Fixed
 * here to match that pattern; see docs/AVAUI_FASE18_CONTROLS.md.
 *
 * Usage:
 *   ava_button_demo [output.html]
 *
 * Exit code:
 *   0 = all tests passed
 *   1 = test failed
 */

#include "components/ComponentTree.h"
#include "controls/Button.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"

// Private headers for demo (same pattern as AvauiPipelineDemo.cpp, Fase 14)
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <stdexcept>

namespace {

class TestLogger {
public:
    void Log(const std::string& msg) {
        std::cout << "[ButtonDemo] " << msg << "\n";
    }
    void Error(const std::string& msg) {
        std::cerr << "[ButtonDemo] ERROR: " << msg << "\n";
    }
    void Assert(bool condition, const std::string& msg) {
        if (!condition) {
            Error(msg);
            throw std::runtime_error(msg);
        }
    }
};

TestLogger logger;

}  // anonymous namespace

int main(int argc, char** argv) {
    const std::string outputPath = (argc > 1) ? argv[1] : "button_demo.html";

    try {
        logger.Log("=== Fase 17: Button Control Demo ===");

        // ===== Test 1: Create ComponentTree and Button =====
        logger.Log("Test 1: Create ComponentTree");
        auto tree = std::unique_ptr<avalang::ui::ComponentTree>(
            avalang::ui::ComponentTree::Create());
        logger.Assert(tree.get(), "Failed to create ComponentTree");
        logger.Log("  OK ComponentTree created");

        // Create root container
        auto root = tree->CreateComponent("Column");
        logger.Assert(root, "Failed to create root Column");
        tree->SetRoot(root);
        logger.Log("  OK Root Column created");

        // Create a Button
        auto button = avalang::ui::controls::CreateButton(tree.get(), "Click Me");
        logger.Assert(button, "Failed to create Button");
        root->AddChild(button);
        logger.Log("  OK Button created with text='Click Me'");

        // Verify Button properties
        auto textProp = button->GetProperty("text");
        logger.Assert(textProp && textProp->Type() == avalang::ui::PropertyType::String,
                      "Button missing 'text' property");
        logger.Log("  OK Button text property set correctly");

        // ===== Test 2: Theme Integration =====
        logger.Log("Test 2: Apply Theme");
        auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
            avalang::ui::CreateDefaultThemeProvider()
        );
        logger.Assert(themeProvider.get(), "Failed to create theme provider");

        auto theme = themeProvider->Current();
        logger.Assert(theme, "Failed to get current theme");
        logger.Log("  OK Theme provider created (" + theme->Name() + ")");

        bool themeApplied = avalang::ui::RenderTheme::Apply(tree.get(), theme);
        logger.Assert(themeApplied, "Failed to apply theme");
        logger.Log("  OK Theme applied to ComponentTree");

        auto bgColorProp = button->GetProperty("backgroundColor");
        logger.Assert(bgColorProp, "Button missing theme-applied 'backgroundColor'");
        logger.Log("  OK Button backgroundColor from theme: " + bgColorProp->AsString());

        auto textColorProp = button->GetProperty("textColor");
        logger.Assert(textColorProp, "Button missing theme-applied 'textColor'");
        logger.Log("  OK Button textColor from theme: " + textColorProp->AsString());

        auto fontNameProp = button->GetProperty("fontName");
        logger.Assert(fontNameProp, "Button missing theme-applied 'fontName'");
        logger.Log("  OK Button fontName from theme: " + fontNameProp->AsString());

        // ===== Test 3: Click Callback Binding =====
        logger.Log("Test 3: Bind Click Callback");
        avalang::ui::controls::BindButtonClick(button->Id(), [](avalang::ui::ComponentId) {});
        logger.Log("  OK Click callback bound to Button");
        logger.Log("  OK Callback registration succeeded");

        // ===== Test 4: Layout Engine =====
        logger.Log("Test 4: Run Layout Engine");
        auto layoutEngine = avalang::ui::LayoutEngine::Create();
        logger.Assert(layoutEngine.get(), "Failed to create LayoutEngine");

        avalang::ui::LayoutRect viewport{0.0, 0.0, 400.0, 300.0};
        auto* layoutRoot = layoutEngine->Compute(root, viewport);
        logger.Assert(layoutRoot, "LayoutEngine failed to compute layout");
        logger.Log("  OK Layout computed successfully");

        // ===== Test 5: Render Tree =====
        logger.Log("Test 5: Build Render Tree");
        std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
            avalang::ui::render::IRenderTree::Create());
        logger.Assert(renderTree.get(), "Failed to create RenderTree");
        renderTree->Build(root, layoutEngine.get());
        auto renderRoot = renderTree->Root();
        logger.Assert(renderRoot != nullptr, "Failed to build RenderTree");
        logger.Log("  OK RenderTree built successfully");

        // ===== Test 6: Scene Graph =====
        logger.Log("Test 6: Build Scene Graph");
        std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
            avalang::ui::scene::ISceneGraph::Create());
        logger.Assert(sceneGraph.get(), "Failed to create SceneGraph");
        sceneGraph->Build(renderRoot);
        sceneGraph->UpdateTransforms();
        logger.Assert(sceneGraph->Root() != nullptr, "Failed to build SceneGraph");
        logger.Log("  OK SceneGraph built successfully");

        // ===== Test 7: Render to HTML =====
        logger.Log("Test 7: Render to HTML");
        auto renderer = avalang::ui::IRenderer::Create("html", 400, 300);
        logger.Assert(renderer.get(), "Failed to create HTMLRenderer");

        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, *renderer);

        const char* htmlOut = renderer->GetOutput();
        logger.Assert(htmlOut && *htmlOut, "HTMLRenderer produced empty output");
        std::string html(htmlOut);
        logger.Log("  OK HTML rendered successfully (" + std::to_string(html.length()) + " bytes)");

        // ===== Test 8: Verify HTML contains Button styling =====
        logger.Log("Test 8: Validate HTML Output");
        bool hasButtonStyle = (html.find("background-color") != std::string::npos);
        logger.Assert(hasButtonStyle, "HTML missing button styling");
        logger.Log("  OK HTML contains button styling");

        bool hasText = html.find("Click Me") != std::string::npos;
        logger.Assert(hasText, "HTML missing button text");
        logger.Log("  OK HTML contains button text");

        // Fase 21 (gap cerrado, ver AVAUI_FASE21_RENDER_PRIMITIVES.md):
        // Theme (Fase 16) escribe "borderRadius"=4 (DefaultTheme's
        // spacing_.borderRadiusPx) en el property bag del Button; hasta
        // esta fase RenderCommand.drawRect no tenía dónde ponerlo y
        // HTMLRenderer nunca emitía la propiedad CSS. Regresión
        // explícita para que este gap no se reabra en silencio.
        auto borderRadiusProp = button->GetProperty("borderRadius");
        logger.Assert(borderRadiusProp, "Button missing theme-applied 'borderRadius'");
        bool hasBorderRadiusCSS = (html.find("border-radius: 4px") != std::string::npos);
        logger.Assert(hasBorderRadiusCSS, "HTML missing border-radius CSS for Button (Fase 21 gap reopened)");
        logger.Log("  OK HTML contains border-radius CSS from theme (Fase 21 gap closed)");

        // ===== Save HTML to file =====
        logger.Log("Test 9: Save Output");
        std::ofstream outFile(outputPath);
        logger.Assert(outFile.is_open(), "Failed to open output file: " + outputPath);
        outFile << html;
        outFile.close();
        logger.Log("  OK HTML saved to " + outputPath);

        // ===== Test 10: Verify cleanup =====
        logger.Log("Test 10: Cleanup");
        avalang::ui::controls::UnbindButtonClick(button->Id());
        logger.Log("  OK Callback unbound");

        logger.Log("");
        logger.Log("=== ALL TESTS PASSED ===");
        logger.Log("Phase 17 validation: PASS");

        return 0;

    } catch (const std::exception& e) {
        logger.Error("Test failed: " + std::string(e.what()));
        return 1;
    }
}
