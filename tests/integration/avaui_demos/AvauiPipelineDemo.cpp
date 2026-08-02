// Fase 14 -- entregable central: probar las 11 fases anteriores juntas
// con un archivo .avaui real (no arboles armados a mano en C++).
//
// Pipeline ejercitado:
//   .avaui (texto) -> AvauiParser -> ComponentTree -> LayoutEngine
//   -> IRenderTree -> ISceneGraph -> SceneCommandWalker -> IRenderer (HTML)
//
// Uso: ava_ui_pipeline_demo [ruta/a/archivo.avaui]
//   Sin argumentos, usa ui/tests/fixtures/index.avaui (copia real de
//   testproj/routes/index.avaui) y escribe el HTML resultante a stdout
//   y a ava_ui_pipeline_demo.html en el directorio actual.
//
// Salida != 0 si cualquier paso falla (ParseError, arbol vacio, etc).

#include "parser/AvauiParser.h"
#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "renderer/IRenderer.h"

// Headers privados de ui/src -- este demo vive dentro del target
// avalang_ui_pipeline_demo, que comparte el include path PRIVATE
// (ui/src) del propio modulo (ver ui/CMakeLists.txt).
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
        throw std::runtime_error("cannot open .avaui file: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Cuenta nodos de un ComponentTree recorriendo desde la raiz -- solo
// para el resumen impreso en consola, no forma parte del pipeline.
int CountComponents(avalang::ui::IComponent* node) {
    if (!node) return 0;
    int count = 1;
    for (auto* child : node->Children()) {
        count += CountComponents(child);
    }
    return count;
}

} // namespace

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "fixtures/index.avaui";

    std::string source;
    try {
        source = ReadFile(path);
    } catch (const std::exception& e) {
        std::cerr << "[fase14-demo] " << e.what() << "\n";
        return 1;
    }

    // 1) Parser: .avaui -> ComponentTree
    avalang::ui::parser::ParsedAvaui parsed;
    try {
        parsed = avalang::ui::parser::AvauiParser::Parse(source);
    } catch (const avalang::ui::parser::ParseError& e) {
        std::cerr << "[fase14-demo] ParseError: " << e.what() << "\n";
        return 1;
    }

    avalang::ui::IComponent* root = parsed.tree->Root();
    if (!root) {
        std::cerr << "[fase14-demo] parser produced a null root (should never happen)\n";
        return 1;
    }

    std::cout << "[fase14-demo] parsed OK: " << CountComponents(root)
              << " components, extends=\"" << parsed.extends
              << "\", routes=" << parsed.routes.size()
              << ", properties=" << parsed.properties.size() << "\n";

    // 2) Layout: ComponentTree -> ILayoutNode tree (800x600 viewport,
    // same default IRenderer::Create uses below).
    auto layoutEngine = avalang::ui::LayoutEngine::Create();
    avalang::ui::LayoutRect viewport{0.0, 0.0, 800.0, 600.0};
    avalang::ui::ILayoutNode* layoutRoot = layoutEngine->Compute(root, viewport);
    if (!layoutRoot) {
        std::cerr << "[fase14-demo] LayoutEngine::Compute returned null\n";
        return 1;
    }
    std::cout << "[fase14-demo] layout OK: root rect = " << layoutRoot->Rect().width
              << "x" << layoutRoot->Rect().height << "\n";

    // 3) Render Tree: ComponentTree + LayoutEngine -> IRenderNode tree
    std::unique_ptr<avalang::ui::render::IRenderTree> renderTree(
        avalang::ui::render::IRenderTree::Create());
    renderTree->Build(root, layoutEngine.get());
    auto renderRoot = renderTree->Root();
    if (!renderRoot) {
        std::cerr << "[fase14-demo] IRenderTree::Build produced a null root\n";
        return 1;
    }

    // 4) Scene Graph: IRenderNode tree -> ISceneNode tree
    std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph(
        avalang::ui::scene::ISceneGraph::Create());
    sceneGraph->Build(renderRoot);
    sceneGraph->UpdateTransforms();
    if (!sceneGraph->Root()) {
        std::cerr << "[fase14-demo] ISceneGraph::Build produced a null root\n";
        return 1;
    }

    // 5) Commands + 6) Renderer: SceneGraph -> RenderCommandSink -> HTMLRenderer
    avalang::ui::RenderCommandSink sink;
    auto renderer = avalang::ui::IRenderer::Create("html", 800, 600);
    avalang::ui::SceneCommandWalker::Walk(*sceneGraph, sink, *renderer);

    const char* html = renderer->GetOutput();
    if (!html || !*html) {
        std::cerr << "[fase14-demo] HTMLRenderer::GetOutput returned empty output\n";
        return 1;
    }

    std::cout << "[fase14-demo] pipeline OK: " << sink.GetCommands().size()
              << " render commands, " << std::string(html).size()
              << " bytes of HTML\n";

    std::ofstream out("ava_ui_pipeline_demo.html", std::ios::binary);
    out << html;
    out.close();

    std::cout << "\n----- HTML output -----\n" << html << "\n------------------------\n";
    std::cout << "[fase14-demo] wrote ava_ui_pipeline_demo.html\n";
    return 0;
}
