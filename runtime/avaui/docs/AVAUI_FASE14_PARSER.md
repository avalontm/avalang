# AVAUI -- Fase 14: Parser `.avaui` -> ComponentTree

**CERRADA.** El entregable central (demo end-to-end) que había quedado
pendiente en la entrega parcial anterior ya está escrito, compilado y
corrido contra un archivo real. Ver `docs/AVALANG_UI_PROGRESS.md` fila
14.

## Hecho

- `ui/src/parser/AvauiParser.h/.cpp`: parser standalone, no toca
  `core/src/ui` (Fase 12). Indentación significativa. Bloques:
  extends/route/import/properties/state/view/code/style, legacy
  metadata/methods. Shorthand `button Id`. Llamada `Navbar()` marcada
  `__unresolvedImportCall` (resolución real: fase futura, fuera de
  alcance de la 14). Política de error: estructural = ParseError duro;
  semántico (tipo desconocido, propiedad no leída) = blando, igual que
  el fallback ya usado por LayoutEngine.
- Gap del alias `gap`/`value` vs `spacing`/`text` (ver entrega anterior):
  resuelto en el parser, sigue igual.
- `ui/src/commands/SceneCommandWalker.h/.cpp`: walker Scene Graph ->
  RenderCommand -> IRenderer, sin cambios de diseño respecto a la
  entrega anterior.
- **`ui/tests/AvauiPipelineDemo.cpp` (nuevo)**: el demo end-to-end que
  faltaba. Corre `.avaui` (texto) -> `AvauiParser` -> `ComponentTree` ->
  `LayoutEngine` -> `IRenderTree` -> `ISceneGraph` ->
  `SceneCommandWalker` -> `IRenderer` ("html") -> HTML final, e imprime
  un resumen de cada paso (nº de componentes, tamaño del layout root,
  nº de render commands, bytes de HTML) más el HTML completo. Falla con
  código de salida != 0 si cualquier paso devuelve null/lanza
  `ParseError`.
- **Fixture real**: `ui/tests/fixtures/index.avaui`, copia textual de
  `testproj/routes/index.avaui` (no un archivo sintético inventado para
  la demo) -- exactamente el caso que pedía el plan ("con un archivo
  .avaui real").
- **Corrido de verdad en este sandbox** (sin toolchain MSVC/vcpkg
  disponible acá, pero el parser y el resto del pipeline no dependen de
  Windows ni de ANTLR): compilado con
  `g++ -std=c++20 -Iinclude -Isrc` contra las fuentes de las Fases 2,
  3, 6, 7, 8, 9, 10 y 14 (con `libglm-dev` instalado vía apt para la
  Fase 7), enlazado y ejecutado. Salida real:
  ```
  [fase14-demo] parsed OK: 7 components, extends="layouts.main", routes=0, properties=1
  [fase14-demo] layout OK: root rect = 800x600
  [fase14-demo] pipeline OK: 7 render commands, 1577 bytes of HTML
  [fase14-demo] wrote ava_ui_pipeline_demo.html
  ```
  Las 11 fases anteriores quedan probadas juntas, por primera vez,
  contra un `.avaui` real en vez de árboles armados a mano en C++.

## Bug real encontrado corriendo la demo (no listado en ningún plan)

La primera corrida no dio 7 comandos sino **17**, y el HTML mostraba
**todo** -- el `Page`, la `Column`, cada `Row`, cada nodo de texto --
como un rectángulo negro sólido y opaco, no solo los tres botones que
sí deberían pintar una caja visible.

Causa raíz, en `ui/src/render_tree/RenderNode.h` (Fase 6, no en
ningún archivo de esta fase): `shouldFill_`/`shouldStroke_`
defaulteaban a `true` incondicionalmente, y `bgColor_`/`borderColor_`
defaulteaban a `""` (el propio comentario del campo decía "default:
transparent"/"default: black"). Pero `common::ParseColor()` (Fase 13)
trata cualquier string vacío o inválido como **fallback negro
opaco** (`Color{0,0,0,255}`, ver `ColorParse.cpp`) -- una decisión
correcta para un color *mal escrito*, pero que aquí se disparaba para
*todo* nodo que simplemente nunca declaró un color, porque nada en
`RenderTree.cpp` (Fase 6) ponía `shouldFill_`/`shouldStroke_` en
`false` cuando no había color que pintar.

Arreglado sin tocar ninguna de las 9 interfaces congeladas (Fase 13):

- `RenderNode.h`: `shouldFill_`/`shouldStroke_` ahora defaultean a
  `false` -- un nodo solo pinta caja si alguien la pidió
  explícitamente.
- `RenderTree.cpp`: cada punto que ya leía `backgroundColor`/
  `borderColor`/`borderWidth` del property bag ahora también llama
  `SetShouldFill(true)`/`SetShouldStroke(true)` cuando encuentra el
  valor -- en `BuildComponent` (colores comunes), `DecomposeContainer`
  (background de un contenedor genérico) y el rectángulo de fondo
  hardcodeado de `DecomposeButton` (que sí quiere pintarse siempre,
  ahora lo pide explícito en vez de heredarlo gratis del default
  viejo).

Con el fix: 7 comandos (antes 17), y el HTML solo pinta caja donde
corresponde -- los tres botones con su fondo gris claro (`#f0f0f0`) y
borde (`#666666`), nada más. Verificado corriendo la demo de nuevo con
el mismo fixture.

## Limpieza adicional encontrada (no relacionada con el bug de arriba)

`ui/include/avalang/ui/render/` y `ui/src/render/` seguían presentes
en el árbol -- versión pre-rename (`ava::ui`, antes del Fase 12) que
`docs/AVALANG_UI_PROGRESS.md` fila 13 ya daba por renombrada a
`render_tree/`. No los referenciaba ni `ui/CMakeLists.txt` ni ningún
otro `.cpp`/`.h` del repo (confirmado por grep) -- código muerto,
duplicado y con el namespace viejo, dejado sin querer al hacer el
rename cosmético en su momento. Eliminados ambos directorios.

## Qué NO hace todavía este demo (documentado a propósito)

- No ejecuta `state`/`code` -- `counter` nunca se resuelve, el nodo de
  texto termina mostrando el string crudo `"Count: " + counter` tal
  cual estaba escrito en el `.avaui` (comportamiento "blando"
  documentado en `AvauiParser.h`: expresiones/bindings de estado son
  fase futura). Visible en el HTML de salida, no es un bug de esta
  fase.
- No resuelve `extends`/`import` (`layouts.main` queda como string sin
  interpretar) -- explícitamente fuera de alcance de la 14.
- No compilado con MSVC real (sin toolchain Windows en este sandbox);
  la validación de esta fase es `g++ -std=c++20`, ejecución completa
  incluida (no solo `-fsyntax-only` como en fases anteriores).
- Sin hit-testing/interacción -- el demo es de solo lectura (parser ->
  HTML), no abre ventana ni procesa eventos. Eso sigue siendo la Fase
  17.
