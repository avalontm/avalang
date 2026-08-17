#ifndef AVA_UI_COMMANDS_SCENECOMMANDWALKER_H
#define AVA_UI_COMMANDS_SCENECOMMANDWALKER_H

#include "renderer/IRenderer.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommandSink.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {

class AVA_UI_API SceneCommandWalker {
public:

    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer);

    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                     std::string slotContent);
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMMANDS_SCENECOMMANDWALKER_H
