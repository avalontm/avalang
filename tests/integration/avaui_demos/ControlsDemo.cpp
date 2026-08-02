/**
 * Fase 18 -- Controls: rest of base set. Demo & validation.
 *
 * Same pattern as ButtonDemo.cpp (Fase 17): build a tree with one of
 * each new control, apply Theme, run Layout -> RenderTree -> SceneGraph
 * -> HTMLRenderer, and assert on the resulting HTML + property state.
 *
 * Controls covered: Text, Image, Column/Row/Stack (Container), TextBox,
 * CheckBox, RadioButton (x2, same group, to validate mutual exclusion).
 *
 * Usage: ava_controls_demo [output.html]
 * Exit code: 0 = all tests passed, 1 = a test failed.
 */

#include "components/ComponentTree.h"
#include "controls/Text.h"
#include "controls/Image.h"
#include "controls/Container.h"
#include "controls/TextBox.h"
#include "controls/CheckBox.h"
#include "controls/RadioButton.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"

#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <stdexcept>

namespace {

class TestLogger {
public:
    void Log(const std::string& msg) { std::cout << "[ControlsDemo] " << msg << "\n"; }
    void Error(const std::string& msg) { std::cerr << "[ControlsDemo] ERROR: " << msg << "\n"; }
    void Assert(bool condition, const std::string& msg) {
        if (!condition) {
            Error(msg);
            throw std::runtime_error(msg);
        }
    }
};

TestLogger logger;

} // anonymous namespace

int main(int argc, char** argv) {
    const std::string outputPath = (argc > 1) ? argv[1] : "controls_demo.html";
    using namespace avalang::ui;
    using namespace avalang::ui::controls;

    try {
        logger.Log("=== Fase 18: Controls Demo (rest of base set) ===");

        // ===== Test 1: build a tree with one of each control =====
        logger.Log("Test 1: Build ComponentTree with all Fase 18 controls");
        auto tree = std::unique_ptr<ComponentTree>(ComponentTree::Create());
        logger.Assert(tree.get(), "Failed to create ComponentTree");

        IComponent* root = CreateColumn(tree.get());
        logger.Assert(root, "Failed to create root Column");
        tree->SetRoot(root);
        SetContainerSpacing(root, 8.0);

        IComponent* title = CreateText(tree.get(), "Fase 18 controls");
        logger.Assert(title, "Failed to create Text");
        root->AddChild(title);

        IComponent* row = CreateRow(tree.get());
        logger.Assert(row, "Failed to create Row");
        root->AddChild(row);

        IComponent* avatar = CreateImage(tree.get(), "@icons/avatar.png");
        logger.Assert(avatar, "Failed to create Image");
        row->AddChild(avatar);

        IComponent* stack = CreateStack(tree.get());
        logger.Assert(stack, "Failed to create Stack");
        row->AddChild(stack);

        IComponent* textBox = CreateTextBox(tree.get(), "Type here...");
        logger.Assert(textBox, "Failed to create TextBox");
        root->AddChild(textBox);

        IComponent* checkBox = CreateCheckBox(tree.get(), "Accept terms", false);
        logger.Assert(checkBox, "Failed to create CheckBox");
        root->AddChild(checkBox);

        IComponent* radioA = CreateRadioButton(tree.get(), "Option A", "demoGroup", true);
        IComponent* radioB = CreateRadioButton(tree.get(), "Option B", "demoGroup", false);
        logger.Assert(radioA && radioB, "Failed to create RadioButtons");
        root->AddChild(radioA);
        root->AddChild(radioB);

        logger.Log("  OK all 8 controls created (Text, Image, Row, Stack, TextBox, CheckBox, 2x RadioButton)");

        // ===== Test 2: control-specific behavior (no rendering yet) =====
        logger.Log("Test 2: Control behavior (value setters, toggles, group exclusivity)");

        SetTextValue(title, "Fase 18 controls (updated)");
        logger.Assert(title->GetProperty("text")->AsString() == "Fase 18 controls (updated)",
                      "SetTextValue did not update Text component");
        logger.Log("  OK Text::SetTextValue");

        bool textBoxChanged = false;
        std::string lastValue;
        BindTextBoxChange(textBox->Id(), [&](ComponentId, const std::string& v) {
            textBoxChanged = true;
            lastValue = v;
        });
        SetTextBoxValue(textBox, "hello");
        logger.Assert(textBoxChanged && lastValue == "hello", "TextBox change callback did not fire correctly");
        logger.Assert(GetTextBoxValue(textBox) == "hello", "GetTextBoxValue mismatch");
        logger.Log("  OK TextBox value + change callback");

        bool newChecked = ToggleCheckBox(checkBox);
        logger.Assert(newChecked == true, "ToggleCheckBox should have flipped false -> true");
        logger.Assert(GetCheckBoxChecked(checkBox) == true, "GetCheckBoxChecked mismatch after toggle");
        logger.Log("  OK CheckBox toggle");

        logger.Assert(GetRadioButtonSelected(radioA) == true, "radioA should start selected");
        logger.Assert(GetRadioButtonSelected(radioB) == false, "radioB should start unselected");
        SelectRadioButton(radioB);
        logger.Assert(GetRadioButtonSelected(radioB) == true, "radioB should be selected after SelectRadioButton");
        logger.Assert(GetRadioButtonSelected(radioA) == false, "radioA should be deselected (group exclusivity)");
        logger.Log("  OK RadioButton group mutual exclusion");

        // ===== Test 3: Theme integration =====
        logger.Log("Test 3: Apply Theme");
        auto themeProvider = std::unique_ptr<IThemeProvider>(CreateDefaultThemeProvider());
        logger.Assert(themeProvider.get(), "Failed to create theme provider");
        ITheme* theme = themeProvider->Current();
        logger.Assert(RenderTheme::Apply(tree.get(), theme), "RenderTheme::Apply failed");

        logger.Assert(textBox->GetProperty("backgroundColor") != nullptr, "TextBox missing theme backgroundColor");
        logger.Assert(textBox->GetProperty("borderColor") != nullptr, "TextBox missing theme borderColor");
        logger.Assert(checkBox->GetProperty("borderColor") != nullptr, "CheckBox missing theme borderColor");
        logger.Assert(radioA->GetProperty("borderColor") != nullptr, "RadioButton missing theme borderColor");
        logger.Assert(title->GetProperty("textColor") != nullptr, "Text missing theme textColor");
        logger.Log("  OK Theme applied to all Fase 18 controls");

        // ===== Test 4: full pipeline to HTML =====
        logger.Log("Test 4: Layout -> RenderTree -> SceneGraph -> HTML");

        root->SetProperty("width", PropertyValue(500.0));
        root->SetProperty("height", PropertyValue(400.0));

        auto layoutEngine = LayoutEngine::Create();
        LayoutRect viewport{0.0, 0.0, 500.0, 400.0};
        auto* layoutRoot = layoutEngine->Compute(root, viewport);
        logger.Assert(layoutRoot != nullptr, "LayoutEngine::Compute failed");

        std::unique_ptr<render::IRenderTree> renderTree(render::IRenderTree::Create());
        renderTree->Build(root, layoutEngine.get());
        auto renderRoot = renderTree->Root();
        logger.Assert(renderRoot != nullptr, "IRenderTree::Build failed");

        std::unique_ptr<scene::ISceneGraph> sceneGraph(scene::ISceneGraph::Create());
        sceneGraph->Build(renderRoot);
        sceneGraph->UpdateTransforms();
        logger.Assert(sceneGraph->Root() != nullptr, "ISceneGraph::Build failed");

        auto renderer = IRenderer::Create("html", 500, 400);
        RenderCommandSink sink;
        SceneCommandWalker::Walk(*sceneGraph, sink, *renderer);

        const char* htmlOut = renderer->GetOutput();
        logger.Assert(htmlOut && *htmlOut, "HTML output empty");
        std::string html(htmlOut);
        logger.Log("  OK pipeline produced " + std::to_string(html.size()) + " bytes of HTML");

        // ===== Test 5: validate HTML contains expected markers =====
        logger.Log("Test 5: Validate HTML output");
        logger.Assert(html.find("Fase 18 controls (updated)") != std::string::npos,
                      "HTML missing updated Text content");
        logger.Assert(html.find("hello") != std::string::npos, "HTML missing TextBox value");
        logger.Assert(html.find("Accept terms") != std::string::npos, "HTML missing CheckBox label");
        logger.Assert(html.find("Option A") != std::string::npos, "HTML missing RadioButton A label");
        logger.Assert(html.find("Option B") != std::string::npos, "HTML missing RadioButton B label");
        logger.Assert(html.find("avatar.png") != std::string::npos, "HTML missing Image src");
        logger.Log("  OK HTML contains all expected control output");

        // ===== Test 6: RadioButton renders as a circle, not a square =====
        // Fase 21 (gap cerrado, ver AVAUI_FASE18_CONTROLS.md sección 2 y
        // AVAUI_FASE21_RENDER_PRIMITIVES.md): CheckBox y RadioButton
        // compartían el mismo lenguaje visual (cuadrado) porque el
        // renderer no tenía primitiva de elipse. RadioButton
        // (DecomposeRadioButton) ahora emite RenderNodeType::Ellipse
        // para su indicador -> SceneCommandWalker lo despacha a
        // sink.DrawEllipse -> HTMLRenderer::OnDrawEllipse, que se
        // reconoce en el HTML por "border-radius: 50%" (CheckBox nunca
        // emite ese valor, sigue siendo un <div> rectangular normal).
        logger.Log("Test 6: RadioButton indicator renders as a circle (Fase 21 gap closed)");
        bool hasEllipse = (html.find("border-radius: 50%") != std::string::npos);
        logger.Assert(hasEllipse, "HTML missing circular indicator for RadioButton (Fase 21 gap reopened)");
        logger.Log("  OK HTML contains a circular (border-radius: 50%) RadioButton indicator");

        std::ofstream out(outputPath);
        logger.Assert(out.is_open(), "Failed to open output file: " + outputPath);
        out << html;
        out.close();
        logger.Log("  OK HTML saved to " + outputPath);

        UnbindTextBoxChange(textBox->Id());

        logger.Log("");
        logger.Log("=== ALL TESTS PASSED ===");
        logger.Log("Phase 18 validation: PASS");
        return 0;

    } catch (const std::exception& e) {
        logger.Error("Test failed: " + std::string(e.what()));
        return 1;
    }
}
