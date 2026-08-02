#ifndef AVA_UI_COMMON_COLOR_PARSE_H
#define AVA_UI_COMMON_COLOR_PARSE_H

#include "commands/RenderCommand.h"
#include <string>

namespace avalang {
namespace ui {
namespace common {

// Fase 13 (freeze de interfaces) encontro que dos fases ya publicadas
// representan color de forma distinta y sin ningun punto de conversion
// entre ellas:
//   - IRenderNode (Fase 6): BackgroundColor()/BorderColor()/ForegroundColor()
//     devuelven std::string en formato CSS-like ("#RGB", "#RRGGBB",
//     "#RRGGBBAA") -- copiado tal cual del property bag de IComponent,
//     pensado para que un .avaui/theme pueda escribir colores como texto.
//   - RenderCommand (Fase 8): usa Color{uint8 r,g,b,a} -- pensado para
//     que el renderer no tenga que parsear strings en el hot path de
//     dibujo.
//
// Ninguna de las dos interfaces publicadas se rompe (romperlas ahora
// no aporta nada: la de Fase 6 es la forma natural de que Theme/.avaui
// escriban colores, la de Fase 8 es la forma natural de que un
// renderer los consuma rapido). Lo que faltaba -- y es lo que esta
// funcion fija -- es un unico punto de conversion entre ambas, para
// que el futuro "walker" que recorra Render Tree/Scene Graph y emita
// RenderCommand (todavia no escrito -- ver docs/AVAUI_FASE13_INTERFACE_FREEZE.md)
// no tenga que inventar su propio parser de hex, ni HTMLRenderer
// (Fase 10) o GdiRenderer (Fase 11) inventar el suyo por separado.
//
// El prefijo "#" es opcional: "F3F3F3" y "#F3F3F3" son equivalentes.
// Esto es necesario porque ThemeColor (ITheme.h) y DefaultTheme.cpp
// almacenan sus valores como "RRGGBB" sin "#" (p.ej. ThemeColor("F3F3F3")),
// y ese mismo string llega sin transformar hasta esta funcion via
// RenderTheme::ApplyTypeDefaults -> IComponent property bag ->
// IRenderNode::BackgroundColor()/BorderColor()/ForegroundColor().
//
// Casos invalidos (string vacio, formato no reconocido, digitos no
// hex): devuelve Color{0,0,0,255} (negro opaco) -- mismo tipo de
// fallback "generico y silencioso" que ya usa LayoutEngine con
// TypeName no reconocido (ver Fase 3), para que un color mal escrito
// en un .avaui no tire abajo el pipeline entero.
Color ParseColor(const std::string& hex);

} // namespace common
} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMMON_COLOR_PARSE_H
