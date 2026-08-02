#ifndef AVA_UI_COMMANDS_SCENECOMMANDWALKER_H
#define AVA_UI_COMMANDS_SCENECOMMANDWALKER_H

#include "renderer/IRenderer.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommandSink.h"
#include "Export.h"
#include <string>

namespace avalang {
namespace ui {

// Fase 13 (freeze de interfaces) confirmo que este walker todavia no
// existia en el repo ("IRenderCommandSink sin productores", ver
// docs/AVAUI_FASE13_INTERFACE_FREEZE.md) y lo dejo anotado para la
// Fase 17 (donde controls/ necesita hit-testing real sobre estos
// nodos). El propio entregable de la Fase 14
// (AVAUI_PLAN_FASE12_PLUS.md: ".avaui -> ... -> Scene Graph -> Commands
// -> HTMLRenderer -> HTML") exige un walker funcionando *ahora* para
// tener el pipeline de 11 fases probado de punta a punta con un
// archivo real -- por eso se escribe aca, deliberadamente minimo.
//
// Lo que hace: recorre la escena en orden de render (pintor) y emite,
// por cada nodo con un IRenderNode asociado, un DrawRectangle (si
// ShouldFill/ShouldStroke) y/o un DrawText/DrawImage segun el tipo de
// nodo.
//
// Lo que NO hace (documentado para que la Fase 17 no lo de por hecho):
//   - No aplica el WorldTransform de escena (Fase 7) mas alla de sumar
//     la posicion (x,y) del propio LayoutRect -- sin escala/rotacion
//     compuesta ni PushClipRect real desde ClipBounds().
//   - Nodos invisibles (IsVisible() == false) no emiten sus propios
//     comandos, pero sus hijos SI se recorren igual (ForEachInRenderOrder
//     no da control de "saltar subarbol") -- una escena real con
//     visibilidad heredada necesita esa poda en la Fase 17.
//   - Sin hit-testing (ese es explicitamente el trabajo de la Fase 17).
//
// Nota de lifetime: IRenderNode::Text()/ImagePath() devuelven
// std::string *por valor* (no referencia), pero RenderCommand solo
// guarda un const char* crudo (ver comentario de "Lifetime is caller's
// responsibility" en RenderCommandSink.cpp). Por eso Walk() recibe
// tambien el IRenderer y hace sink -> ProcessCommands en la misma
// llamada: el storage interno que respalda esos punteros solo
// necesita vivir dentro de Walk(), nunca cruzar la firma publica.
// Partir "construir el batch" de "consumirlo" en dos llamadas
// publicas obligaria a exponer ese storage; si una fase futura
// necesita el batch sobreviviendo mas alla de esta llamada, ese es el
// momento de separarlas.
class AVA_UI_API SceneCommandWalker {
public:
    // Recorre `scene` completa emitiendo comandos en `sink`, y los
    // entrega a `renderer` (BeginFrame/ProcessCommands/EndFrame en
    // `renderer`; BeginFrame/EndFrame tambien en `sink`, para dejar su
    // stack de clipping en un estado limpio). No falla si `scene` no
    // tiene Root() -- produce un frame vacio.
    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer);

    static void Walk(scene::ISceneGraph& scene, RenderCommandSink& sink, IRenderer& renderer,
                     std::string slotContent);
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMMANDS_SCENECOMMANDWALKER_H
