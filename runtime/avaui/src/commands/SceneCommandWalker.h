#pragma once

#include "renderer/IRenderer.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommandSink.h"
#include "theme/ProjectStyleOverrides.h"
#include "Fwd.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {

struct InteractiveState {
    const theme::ProjectStyleSheet* styles = nullptr;
    int pointerX = 0;
    int pointerY = 0;
    bool pointerDown = false;
    ComponentId focused = 0;
};

class AVA_UI_API SceneCommandWalker {
public:

    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                     const InteractiveState* interactive = nullptr);

    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                     std::string slotContent, const InteractiveState* interactive = nullptr);
};

}
}