// Fase 20.2 validation -- ejercita 3 sub-fases del swap al motor nuevo.
// Compilado con `g++ -std=c++20` igual que AvauiPipelineDemo. Corre:
//   1) imports_test.avaui (20.2.3) -- valida parse de `import components.card`
//   2) extends_test.avaui  (20.2.2) -- valida parse de `extends layouts.main`
//   3) state_cache_test.avaui (20.2.4) -- valida parse de `state`/`code`/`click`
//
// Los sub-fases 20.2.1 (title/extraHead/extraBodyEnd) y 20.2.5 (swap en
// app.cpp) requieren el adapter de avahost/ y no se ejercitan aqui.
//
// Limit: este demo NO resuelve imports ni compone layouts (esos viven en
// UiComponentResolver y RenderAvauiDynamicWithLayoutAndState, que son
// avahost/-only). Solo confirma que los fixtures parsean + renderizan
// el contenido propio sin fallar.

#include "parser/AvauiParser.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"

#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int RenderPipeline(const std::string& source, std::string& outHtml, std::string& outError) {
    outHtml.clear();
    outError.clear();
    try {
        auto parsed = avalang::ui::parser::AvauiParser::Parse(source);
        if (!parsed.tree || !parsed.tree->Root()) {
            outError = "empty tree";
            return 1;
        }
        auto layoutEngine = avalang::ui::LayoutEngine::Create();
        avalang::ui::LayoutRect viewport{0.0, 0.0, 1280.0, 720.0};
        layoutEngine->Compute(parsed.tree->Root(), viewport);

        std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
            avalang::ui::render::IRenderTree::Create());
        renderTree->Build(parsed.tree->Root(), layoutEngine.get());

        std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
            avalang::ui::scene::ISceneGraph::Create());
        sceneGraph->Build(renderTree->Root());
        sceneGraph->UpdateTransforms();

        auto renderer = avalang::ui::IRenderer::Create("html", 1280, 720);
        if (!renderer) {
            outError = "IRenderer::Create failed";
            return 1;
        }
        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, *renderer);
        const char* html = renderer->GetOutput();
        outHtml = html ? html : "";
        return 0;
    } catch (const std::exception& e) {
        outError = e.what();
        return 1;
    }
}

int RunBlock(const std::string& name, const std::string& fixturePath,
               const std::string& mustContain) {
    std::cout << "[fase20.2-demo] === " << name << " (" << fixturePath << ") ===\n";
    std::string source;
    try {
        source = ReadFile(fixturePath);
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: " << e.what() << "\n";
        return 1;
    }
    std::string html, err;
    int rc = RenderPipeline(source, html, err);
    if (rc != 0) {
        std::cerr << "  FAIL: " << err << "\n";
        return 1;
    }
    if (!Contains(html, mustContain)) {
        std::cerr << "  FAIL: expected output to contain '" << mustContain
                  << "', got " << html.size() << " bytes\n";
        return 1;
    }
    std::cout << "  OK: " << html.size() << " bytes, contains '"
              << mustContain << "'\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string fixturesDir = (argc > 1) ? std::string(argv[1])
                                         : std::string("tests/fixtures/fase20_2");

    int failures = 0;

    failures += RunBlock("20.2.3 imports",
                         fixturesDir + "/imports_test.avaui",
                         "Click me");

    failures += RunBlock("20.2.2 extends",
                         fixturesDir + "/extends_test.avaui",
                         "Page content here");

    failures += RunBlock("20.2.4 state cache",
                         fixturesDir + "/state_cache_test.avaui",
                         "Counter widget");

    if (failures == 0) {
        std::cout << "[fase20.2-demo] ALL OK (3/3 static fixtures parse + render)\n";
        return 0;
    }
    std::cerr << "[fase20.2-demo] " << failures << " failure(s)\n";
    return 1;
}
