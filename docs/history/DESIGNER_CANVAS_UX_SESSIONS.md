# Historia — UX del Canvas del Designer (sesiones de implementación)

Bitacora historica, sesion por sesion, de las fases 7-11 de mejoras de
interaccion y estetica del canvas visual (`designer_canvas.cpp`). Es un
registro de proceso, no arquitectura vigente: para el estado actual del
canvas ver `docs/architecture/16_STUDIO.md`.

Movido desde `docs/architecture/09_DESIGNER_CANVAS_UX_PLAN.md` como
parte de la reorganizacion de la documentacion.

**Advertencia**: las marcas de estado ("Hecho"/"Pendiente") dentro de
este documento reflejan el momento en que se escribio cada sesion, no
el estado actual del proyecto.

---

# Plan — Interacción y estética del canvas del Designer (AvaStudio)

Objetivo: que el canvas visual (`designer_canvas.cpp`, vista Design de
un `.avaui`) sea usable de verdad — seleccionar cualquier componente
(incluidos contenedores con hijos), reordenarlo por drag, ver/editar
sus propiedades, y borrarlo — y que se vea como un editor visual, no
como un wireframe de debug.

Este documento asume el estado descrito en
`08_DESIGNER_VIEW_PLAN.md` (Fases 0-6 + Anexo 9.19, todas ✅) como
punto de partida. Lo de acá son fases nuevas (7+) sobre ESE código.

**Nota de idioma:** este documento (y los comentarios de sesiones
anteriores) están en español porque es como se discute el proyecto,
pero el IDE en sí es 100% en inglés — todo string visible en la UI
(`explorer_panel.cpp`: `"Delete"`, `"Rename"`, `"New File"`, etc.) ya
está así. Cualquier texto nuevo que salga de este plan (menú
contextual, breadcrumb, tooltips) debe seguir esa misma convención:
código/UI en inglés, comentarios y este plan en español. Ej.: el ítem
de menú de Fase 8 es `"Delete"` (no "Eliminar").

---

## Estado de avance (tabla de progreso)

Convención de estado (misma que `08_DESIGNER_VIEW_PLAN.md`):
- ✅ **Hecho** — implementado y probado.
- 🟡 **Parcial** — una parte está hecha, falta algo concreto (ver "Qué falta").
- 🔲 **Pendiente** — no arrancado.

| # | Fase / Ítem | Estado | Qué falta | Detalle |
|---|---|---|---|---|
| Fase 7 | Franja de header propia por contenedor (fix de hit-testing) | ✅ Hecho | — | `kHeaderHeight` (20px) + `extra_offset_y` en `DrawNode`, ver Diagnóstico punto 1 |
| Fase 7 | Breadcrumb de jerarquía (subir de hijo a ancestro) | ✅ Hecho | — | `DrawBreadcrumbBar`, barra fija sobre el canvas (fuera del `BeginChild`) |
| Fase 8 | `RemoveNode` en `design_document.h`/`.cpp` | ✅ Hecho | — | Mismo patrón de splice que `MoveNode`; no borra root ni nodos sintéticos |
| Fase 8 | Tecla Delete/Supr sobre nodo seleccionado | ✅ Hecho | — | Gateado por foco de la child window (mismo patrón que `explorer_panel.cpp`) |
| Fase 8 | Menú contextual (click derecho) con `"Delete"` + confirmación | ✅ Hecho | — | `BeginPopupContextItem` sobre el header/hit-area + popup Yes/Cancel (`DrawCanvasDeleteConfirmPopup`) |
| Fase 9 | Línea/indicador de inserción durante el drag | ✅ Hecho | — | Ya lo resolvía el peek-only `AcceptDragDropPayload` de `HandleDropTarget` (preexistente a este plan) — Fase 7 no lo tocó y seguía funcionando; no hizo falta código nuevo |
| Fase 9 | Drop confiable "como último hijo" de un contenedor ya poblado | ✅ Hecho | — | Resuelto por Fase 7: `ComputeDropZone` ya devuelve `kInto` siempre que el hit-rect es el header de un contenedor |
| Fase 10 | Render con widgets reales de ImGui por `node.type` (siempre, no atado a Preview) | ✅ Hecho | — | `DrawRealWidget` en `designer_canvas.cpp`: button/textbox/checkbox/radiobutton/text/link/divider/spacer/image |
| Fase 10 | Capa de selección encima del widget real | 🟡 Parcial | Ver "Desvío de diseño" abajo — Paso A hecho; Paso B tiene el experimento listo (`AVA_FASE10_PASO_B_MODE` en `designer_canvas.cpp`) pero sin ejecutar — este entorno no tiene acceso a red para compilar (`FetchContent` de GLFW/ImGui) | Fase 10 |
| Fase 10 | Márgenes legibles a mayor profundidad de anidamiento | ✅ Hecho | — | `kNodeMarginPerDepth`/`kNodeMarginMax`, nuevo parámetro `depth` en `DrawNode` |
| Fase 10 | Overlay/halo de selección + cursor "mano" en hover | ✅ Hecho | — | Halo + 4 handles de esquina; cursor mano ahora en leaves también, no solo containers |
| Fase 10.1 | Deprecar/eliminar el modo Preview del Anexo 9.19 (toggle, banner, consola) | ✅ Hecho | — | `SetDesignerPreviewActive` y compañía, `PreviewLogLine`, el banner y la barra/consola de `editor_panel.cpp` fueron removidos esta sesión |
| Fase 11 | Undo/redo acotado al canvas (stack de snapshots de `doc.root`) | 🔲 Pendiente | Todo — opcional, no bloqueante para el resto | Sección "Fase 11" |

**Nota sobre esta sesión:** no se compiló en ningún momento — no por elección, sino porque este entorno no tiene acceso a red y el build de `ava_studio` depende de `FetchContent` clonando GLFW/ImGui desde GitHub en configure-time. Todo lo de arriba está escrito y revisado a mano contra el código existente, no verificado contra un build real. Esta sesión agregó el Paso A de Fase 10 (desacoplar seguridad de fidelidad visual en `DrawRealWidget`, ver "Desvío de diseño") — riesgo de compilación bajo pero no nulo (`ImGuiStyleVar_DisabledAlpha`, no confirmado contra la versión vendorizada) — y dejó listo el experimento de Paso B (macro `AVA_FASE10_PASO_B_MODE`, misma sección) para que se ejecute con un build local. El punto más sensible a un error de compilación/comportamiento sigue siendo ese Paso B; Fase 10.1 (sesión anterior) es la otra fuente de riesgo de build, ver esa sección.

---

## Diagnóstico — por qué falla hoy (verificado contra el código real)

No son suposiciones de UX en abstracto; cada punto está anclado a la
línea exacta de `designer_canvas.cpp` que lo causa.

### 1. Un contenedor con hijos es literalmente imposible de seleccionar/arrastrar

`DrawNode` dibuja el `InvisibleButton` del padre sobre TODO su rect
(`p0`→`p1`, línea ~432), y después recorre `node.children` dibujando
el de cada hijo ENCIMA, en el mismo espacio de pantalla (el layout es
"full-bleed, edge-to-edge" — comentario en `kNodeMargin`, línea
~145 — los hijos ocupan el 100% del área disponible del padre, sin
resto). ImGui resuelve overlaps por orden de inserción: el último
`InvisibleButton` agregado gana el click. Como los hijos siempre se
agregan DESPUÉS que el padre, **cualquier click dentro de un
contenedor con al menos un hijo cae sobre ese hijo, nunca sobre el
padre** — no hay ningún pixel del padre que quede "libre" para
clickear. El propio código ya lo marca como límite conocido, sin
resolver ("not stress-tested for pathological overlap", línea ~416).

Consecuencia directa observable: no podés seleccionar un `column`/`row`
para verle sus propias props (`gap`, `padding`, `fill`) ni para
arrastrarlo a otro lugar — sólo podés tocar sus hijos. Si querés mover
todo un bloque, hoy no se puede.

### 2. Reordenar "dentro de" un contenedor ya poblado tampoco funciona

`HandleDropTarget` (Fase 4) calcula bandas 25%/50%/25% (`kEdgeBandFrac`,
línea ~229) sobre el rect del nodo que recibe el drop — pero por el
mismo motivo del punto 1, ese rect nunca es el del contenedor una vez
que tiene hijos (el contenedor no gana el hit-test). El único modo de
"entrar" a un contenedor ya poblado es soltar en la banda del 50%
central de alguno de sus hijos que sea a su vez contenedor — no hay
forma de agregarlo como último hijo del padre directamente si el
padre no está vacío.

### 3. No existe ningún comando para borrar un nodo

Se buscó en todo el árbol `RemoveNode`/`DeleteNode`/tecla Delete/menú
contextual sobre el canvas — no hay nada. `explorer_panel.cpp` tiene
Delete pero es para ARCHIVOS, no para nodos dentro de un `.avaui`
abierto. `properties_panel.cpp` sólo tiene "quitar una prop/evento
puntual" (`DrawRemoveButton`), no "borrar el nodo entero". Esto no es
un bug de interacción, es una función que directamente no se escribió
todavía.

### 4. Estética: wireframe uniforme sin affordance real

- Todo nodo se dibuja igual: rect relleno + borde + label de texto
  (`type (id)`) — un `button`, un `checkbox` y un `container` se ven
  como la misma caja con texto distinto adentro (comentario propio del
  plan original, sección 8 pregunta 2, sigue sin resolverse).
- El único feedback de selección es el color del borde
  (`palette::kPrimary` vs `kBorder`) — sin overlay, sin handles, sin
  indicación de "esto se puede arrastrar" al hacer hover.
- `kNodeMargin` = 3px es el único "aire" entre nodos anidados — a
  cualquier nivel de anidamiento razonable (3-4 niveles) la separación
  visual entre padre e hijo prácticamente desaparece.
- Durante un drag no hay ninguna línea/indicador de "se va a insertar
  acá" — el usuario suelta a ciegas y descubre el resultado después.

---

## Plan de fases

### Fase 7 — Arreglar selección: hit-testing por capas + breadcrumb

El fix de fondo para el punto 1 es reservar SIEMPRE un área del padre
que gane el hit-test, sin depender de que sobre espacio "libre":

- **Franja de header en cada contenedor**: en vez de que el
  `InvisibleButton` del padre cubra todo `p0`→`p1` y los hijos lo
  tapen entero, el contenedor dibuja una franja fija arriba (18-20px,
  con su label `type (id)` adentro) que es SU área de selección/drag
  propia y exclusiva — los hijos arrancan debajo de esa franja, nunca
  la superponen. Esto no rompe `layout_engine.cpp` (sigue siendo
  full-bleed para el contenido), sólo resta esos px fijos al área que
  `DrawNode` usa para posicionar a los hijos dentro del padre — cambio
  acotado a `designer_canvas.cpp`, no a `layout_engine.cpp`.
- **Breadcrumb de jerarquía** en Properties (o en una barra fina sobre
  el canvas): "Page > column > row > button" con cada segmento
  clickeable, para subir de un hijo a cualquier ancestro sin depender
  de encontrarle un pixel libre en el canvas. Barato de armar: ya
  existe `DesignNode::node_uid` y el árbol se puede recorrer desde
  `doc.root` buscando el path hasta `doc.selected_uid`.
- Con la franja de header, el contenedor pasa a poder actuar como
  **drag source** con el mismo `InvisibleButton` sin tocar
  `HandleDropTarget` ni `MoveNode` (Fase 4 ya soporta arrastrar
  cualquier nodo real) — arrastrar un contenedor entero pasa a ser tan
  simple como agarrarlo por su header.
- **Nota para Fase 9**: una vez que el header es un área separada del
  resto del contenedor, la lógica de bandas 25%/50%/25% de
  `HandleDropTarget` (pensada para el rect completo de un nodo) ya no
  aplica igual ahí — soltar sobre el header debe interpretarse
  directamente como "kInto" (último hijo), sin banda, mientras que las
  bandas siguen aplicando normalmente sobre el rect de contenido de
  cada hijo (sibling antes/dentro/después). Esto es un ajuste de
  `HandleDropTarget`/su caller, no solo de `DrawNode` — a tener en
  cuenta al implementar Fase 9, no solo Fase 7.

### Fase 8 — Borrar nodos

- `design_document.h`/`.cpp`: nueva `bool RemoveNode(DesignDocument&
  doc, const std::string& node_uid)` — misma familia que `MoveNode`
  (splice del árbol), pero sacar en vez de reinsertar. Mismas reglas
  que `MoveNode`: no se puede borrar `doc.root`, no aplica a nodos
  sintéticos (resueltos de un import).
- `designer_canvas.cpp`: tecla `Delete`/`Supr` con `doc.selected_uid`
  no vacío dispara `RemoveNode` + limpia `doc.selected_uid` +
  invalida `eval_cache` (mismo patrón que ya usa el click de Fase 6
  para limpiar cache después de mutar el árbol).
- Menú contextual (click derecho) sobre el header de cada nodo (Fase
  7 ya le da un área propia y estable): `"Delete"`, con el mismo
  criterio de confirmación que `explorer_panel.cpp` ya usa para
  archivos (popup Yes/Cancel) — reusar `DrawDeleteConfirmPopup` como
  referencia de patrón, no necesariamente el mismo código.
- **No bloqueante para esta fase**: undo. Fase 11 lo cubre aparte.

### Fase 9 — Reorder: feedback visual durante el drag

**Estado: ✅ Hecho — sin código nuevo.** Al revisar el código antes de
tocar nada para esta fase, las dos cosas que este plan pedía ya
estaban resueltas:

- El indicador de inserción durante el drag (línea antes/después, o
  highlight de "va a entrar acá") ya existía en `HandleDropTarget` vía
  un `AcceptDragDropPayload(kNodeMoveDragDropId,
  ImGuiDragDropFlags_AcceptPeekOnly)` que dibuja exactamente esa
  banda cada frame que el mouse está encima de un target durante el
  drag — el diagnóstico original de este punto ("la banda se usa para
  decidir la acción pero nunca se muestra") ya no era así para cuando
  se implementó Fase 7; quedó sin tocar y sigue funcionando igual.
- "Agregar como último hijo de un contenedor ya poblado" quedó
  resuelto como efecto directo de Fase 7: `ComputeDropZone` devuelve
  `kInto` sin banda para cualquier hit-rect de contenedor (que ahora
  ES el header, gracias a Fase 7), exactamente como pedía la "Nota
  para Fase 9" original de este documento.

No se escribió nada específico de Fase 9 — se dejó esta sección para
que quede registrado que se verificó explícitamente, no que se saltó.

### Fase 10 — Controles reales (no wireframe), siempre en vivo

**Estado: ✅ Hecho (widgets, márgenes, halo) / 🟡 Parcial (capa de
selección — ver "Desvío de diseño" abajo).**

No hace falta un modo "Design vs Preview" para la apariencia — el
widget se dibuja con su look real **siempre**. El toggle de Preview
(Anexo 9.19) no se mantiene como alternativa: se elimina en la Fase
10.1 de abajo, porque deja de tener un propósito distinto una vez que
el render en vivo no depende de él. **Fase 10.1 en sí sigue
pendiente** (no se tocó en esta sesión) — el Preview dedicado
(Anexo 9.19) todavía existe en paralelo al render real de esta fase.

- `component_catalog.cpp` mapea casi 1:1 a widgets nativos de ImGui,
  implementado en una función nueva `DrawRealWidget` (`designer_canvas.cpp`):
  `button` → `ImGui::Button`, `textbox` → `ImGui::InputText` (read-only —
  ver limitación abajo), `checkbox` → `ImGui::Checkbox`, `radiobutton`
  → `ImGui::RadioButton`, `text`/`link` → `ImGui::TextUnformatted`/texto
  coloreado + subrayado, `divider` → una línea vía `ImDrawList` (no
  `ImGui::Separator` — ese asume que ocupa el ancho completo de la
  ventana/columna actual, acá se dibuja manual entre `p0.x`/`p1.x` para
  no pelear con el posicionamiento explícito del nodo), `spacer` → nada
  (correcto, un spacer no tiene contenido visible), `image` → un
  placeholder de marco + diagonal cruzada, NO un `ImGui::Image` real
  todavía (**limitación conocida, no resuelta**: no hay pipeline de
  carga de texturas en el Designer — `src` sigue siendo solo un string
  de path). `DrawNode` llama a esta función para cualquier nodo que no
  sea contenedor, en vez de siempre el rect+label genérico; si el tipo
  no tiene mapeo (`DrawRealWidget` devuelve `false`), cae de vuelta a
  un fallback de texto plano (label + valor evaluado) — no debería
  pasar con el catálogo de hoy, pero cubre un tipo futuro/desconocido
  sin mostrar nada.
- El widget se dibuja con el valor YA evaluado (mismo
  `EvalPropertyExpr`/`eval_cache` de Fase 6, sin cambios) — una
  edición en Properties se ve reflejada en el control real al frame
  siguiente, sin pasos intermedios ni que el usuario tenga que "correr"
  nada.
- **Capa de selección encima — implementada CON `BeginDisabled(true)`,
  no sin él.** Ver "Desvío de diseño" justo abajo; este documento
  original asumía que el orden de inserción de ImGui alcanzaba solo,
  y la implementación real no confió en esa suposición sin poder
  compilarla/probarla. **Paso A (esta sesión):** se desacopló el
  "input-disabled por seguridad" (siempre activo) de la "fidelidad
  visual" (atenuado), que hasta ahora eran la misma bandera — ver
  detalle en "Desvío de diseño".
- Correr el `click` handler contra el `state_vm` sigue siendo
  Ctrl+Click (Anexo 9.18) — sin cambios; el modo Preview dedicado
  sigue existiendo en paralelo hasta que Fase 10.1 lo elimine.
- Márgenes/anidamiento: `kNodeMargin` (3px) pasó a ser
  `kNodeMargin + depth * kNodeMarginPerDepth` (clamp a
  `kNodeMarginMax`, 12px) — nuevo parámetro `depth` agregado a la
  firma de `DrawNode`, incrementado en +1 en cada llamada recursiva a
  hijos.
- Halo/overlay de selección: implementado — un rect semitransparente
  ~3px por fuera del propio borde del nodo, más 4 handles cuadrados en
  las esquinas, dibujado además del cambio de color de borde existente
  (no en su lugar). Cursor "mano" en hover: existía solo para
  containers desde Fase 7, se extendió a leaves también en esta
  pasada (un leaf es tan seleccionable/arrastrable como un container).
- **Efecto secundario no pedido explícitamente por el plan, pero
  necesario**: el label `type (id)` que antes se dibujaba arriba de
  TODO nodo ahora solo se dibuja para containers (dentro de su header
  strip, que ya tenía el espacio). Un leaf mide `kDefaultLeafHeight`
  (32px) menos margen — reservar ~20px arriba para el label hubiera
  dejado el widget real reducido a una tira de pocos px de alto,
  contradiciendo el propio objetivo de esta fase. El widget real de un
  leaf ya muestra su propio texto evaluado (el label de un button, de
  un checkbox, etc.), que alcanza como identificación en el uso diario;
  `label` (con `node.id`) se sigue usando en el fallback de texto plano
  para un tipo sin mapeo.

#### Desvío de diseño respecto al plan original: `BeginDisabled(true)`

Este documento (antes de la sesión anterior) afirmaba que alcanzaba
con dibujar el widget real ANTES del `InvisibleButton` de Fase 7, "sin
que haga falta `BeginDisabled()`", asumiendo que el orden de inserción
de ImGui (el último item dibujado en un rect gana el hit-test) bastaba
para que el widget de abajo nunca reaccionara a su propio input.

Esa suposición es correcta para **quién queda con el hover** al final
del frame (eso sí es "último gana" en ImGui), pero no cubre el
**mismo instante en que el mouse baja** sobre el rect: `ButtonBehavior`
(la función interna que usan `Button`/`Checkbox`/`RadioButton`/etc.)
puede asignarle el `ActiveId` de ImGui al widget real en el momento en
que SE PROCESA ÉL, que es antes que el `InvisibleButton` — sin poder
compilar/probar esto, no había forma de confirmar si eso alcanza a
"robarle" el click a la capa de selección de encima o no.

Por eso `DrawRealWidget` envuelve todo el bloque en
`ImGui::BeginDisabled(true)`: un item disabled no participa en la
lógica de `ActiveId` en absoluto, así que el widget de abajo
**garantizado** nunca puede interceptar un click, sin depender de
verificar el orden de submisión. Ese `BeginDisabled(true)` mezclaba dos
cosas que son independientes:

1. **Input-disabled por seguridad** — que el widget real no le robe el
   `ActiveId` a la capa de selección. Esto SIEMPRE debe estar activo,
   sea `enabled: true` o `false` en el catálogo.
2. **Atenuado visual (alpha)** — SOLO debería reflejar el prop real
   `enabled` del nodo, no la mecánica de seguridad de arriba.

`BeginDisabled(true)` forzaba (2) como efecto colateral de (1): un
botón `enabled: true` se veía atenuado igual que uno `enabled: false`,
sin distinguir ese estado (el prop nunca se leía en `DrawRealWidget`).

**Paso A (esta sesión, sin compilar) — desacoplado:**
`ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f)` anula el
atenuado automático que dispara `BeginDisabled(true)`, sin tocar su
garantía de seguridad de `ActiveId`. Encima de eso, se lee el prop real
del nodo (`FindPropValue(node.properties, "enabled", "true")`) y se
aplica un segundo `PushStyleVar(ImGuiStyleVar_Alpha, 0.60f)` solo
cuando ese prop es `"false"` — mismo valor que el `DisabledAlpha` por
defecto de ImGui, así el atenuado se ve idéntico al de cualquier otro
disabled del proyecto, pero ahora es fiel al catálogo. Ambos
`PushStyleVar` se cierran con un único `PopStyleVar(2)` antes del
`EndDisabled()` existente. Nodos sin ese prop (`text`, `link`,
`divider`, etc.) caen en el fallback `"true"` = nunca atenuados, que es
el comportamiento correcto para ellos.

Con esto se cierra la brecha visual que este documento señalaba
("un botón enabled y uno con `enabled: false` se ven casi iguales
hoy") — **sin tocar** la mecánica de `BeginDisabled(true)` que da la
garantía de seguridad de `ActiveId`. Riesgo de este paso: ninguno
conocido para la garantía de seguridad (no se tocó esa lógica); riesgo
de compilación: bajo pero no nulo — `ImGuiStyleVar_DisabledAlpha` no
se pudo confirmar contra la versión de ImGui vendorizada en el repo (no
hay `imgui.h` en el árbol fuente, se trae por CMake/submódulo) — es
API estable desde ImGui 1.87 (`BeginDisabled`/`EndDisabled` ya se usan
en otro lugar del proyecto, así que la versión vendorizada es al menos
esa), así que el riesgo se considera bajo, pero queda como el primer
punto a revisar si el build falla en esta función.

**Paso B (pendiente — requiere build; experimento listo en código,
esta sesión):** el propio entorno de esta sesión no tiene acceso a red
(el build de `ava_studio` trae GLFW/ImGui vía `FetchContent` desde
GitHub en configure-time — sin red no hay forma de compilar acá), así
que no se pudo ejecutar el experimento. Queda preparado en
`designer_canvas.cpp`, justo antes de `DrawRealWidget`, un toggle de
una sola línea:

```cpp
#define AVA_FASE10_PASO_B_MODE 0
```

Cambiar ese `0` y compilar localmente, en este orden:

- **`MODE 1`** — saca el `BeginDisabled(true)` de seguridad por
  completo; solo queda disabled si el prop real del nodo lo pide. Ésta
  es la asunción original del plan (orden de inserción — el
  `InvisibleButton` de Fase 7 dibujado DESPUÉS del widget real —
  alcanza solo). Probar: clickear/arrastrar un button/checkbox/
  radiobutton real en el Designer Canvas. Si sigue
  seleccionando/arrastrando el NODO sin que el widget reaccione a su
  propio click (un checkbox no debería tildarse, por ejemplo), el modo
  pasa la prueba y `BeginDisabled(true)` de seguridad puede eliminarse
  del todo.
- **`MODE 2`** — si `MODE 1` falla (el widget real se roba el
  `ActiveId`), probar este modo: agrega
  `ImGui::SetNextItemAllowOverlap()` antes de dibujar el widget real.
  Si esta línea no compila, el ImGui vendorizado (vía `FetchContent`,
  rama `docking`) no trae esa función todavía — reportar el error de
  compilación tal cual, sin insistir en este modo.
- Si ambos fallan, quedarse en `MODE 0` (el entregado) de forma
  permanente — ya no como suposición sino como resultado confirmado.

Cualquiera sea el resultado, avisar para: (1) dejar la macro fija en
el `MODE` que corresponda y borrar del código los `#elif` que no
aplican, (2) actualizar el comentario de `DrawRealWidget` en
`designer_canvas.cpp`, y (3) actualizar esta sección con el resultado
confirmado en vez de la suposición actual. Fase 10 pasa a ✅ Hecho sin
salvedades recién cuando Paso B esté resuelto.

### Fase 10.1 — Deprecar el modo Preview (Anexo 9.19)

**Estado: ✅ Hecho — implementado esta sesión, sin compilar.** Con
Fase 10 (el widget se ve y actualiza real *siempre*, sin depender de
ningún toggle), el modo Preview dedicado que se armó en el Anexo 9.19
quedó sin propósito — ya no hacía falta un modo aparte para "ver cómo
se vería de verdad", porque eso pasó a ser el estado normal del
canvas. Se eliminó en vez de mantenerse como código muerto:

- **Eliminado:**
  - `SetDesignerPreviewActive` / `IsDesignerPreviewActive` /
    `ResetDesignerPreviewState` / `GetDesignerPreviewLog` /
    `ClearDesignerPreviewLog` (`designer_canvas.h`/`.cpp`).
  - `PreviewLogLine` y los campos `preview_active`/`preview_log` de
    `DesignerVmCacheEntry`, junto con los tres trampolines que
    escribían ahí (`PreviewLogTrampoline`/`PreviewAlertTrampoline`/
    `PreviewNavigateTrampoline`) y su instalación
    (`ava_vm_set_print_callback`/`set_alert_callback`/
    `set_navigate_callback`) en el rebuild de la vm cacheada.
  - El banner "PREVIEW -- los clicks ejecutan handlers reales" sobre
    el canvas (`DrawDesignerCanvas`).
  - La barra "Preview: ON/OFF" + "Reset" + consola scrolleable (con su
    cálculo de `console_height`) en `editor_panel.cpp` (Design view) —
    el canvas vuelve a usar `ImGui::GetContentRegionAvail()` completo,
    sin reservar nada al fondo.
  - La rama `preview_active ||` en `should_invoke_click`
    (`designer_canvas.cpp`) — vuelve a ser exclusivamente Ctrl+Click,
    como estaba antes de 9.19.
- **Conservado** (no era parte de lo que se deprecia, es
  infraestructura de VM/core, no de UI): `VM::AlertSink`/
  `NavigateSink`, `ava_vm_set_alert_callback`/`ava_vm_set_navigate_callback`,
  `ui.alert`/`ui.navigate` en `builtins.cpp`. Eso sigue existiendo en
  el core sin cambios — el Designer simplemente no instala ningún sink
  hoy, ya que no le queda consola donde mostrar ese output; un futuro
  host (o el propio Designer más adelante) puede seguir instalándolos
  si hace falta.
- El aviso de error de un handler que no compila/tira en runtime
  (Anexo 9.18 limitación 2, que 9.19 había resuelto vía `preview_log`)
  vuelve a quedar sin un lugar donde mostrarse ahora que la consola fue
  removida — `handler_error` en el bloque de Ctrl+Click de
  `designer_canvas.cpp` queda capturado y descartado explícitamente
  (`(void)handler_error`, con comentario) en vez de silenciado por
  descuido. No resuelto por este plan; si vuelve a hacer falta, se
  retoma aparte (no bloqueante para Fases 7-10).
- **Pendiente real de esta implementación:** compilar y confirmar que
  no queda ninguna llamada colgante a la API removida en algún caller
  que no sea `editor_panel.cpp`/`main.cpp` (no se encontró ninguna al
  grepear `studio/src/` esta sesión, pero no hay build que lo
  confirme) — mismo pendiente #1 de la sección "Orden recomendado".

### Fase 11 (opcional, no bloqueante) — Undo/redo del canvas

Borrar (Fase 8) y mover (Fase 4, ya existente) son ahora operaciones
destructivas sin forma de deshacer más que Ctrl+Z del editor de texto
(que no aplica a la vista Design). Un stack simple de snapshots de
`doc.root` antes de cada `RemoveNode`/`MoveNode` alcanzaría para un
Ctrl+Z acotado al canvas — se deja aparte porque no bloquea que las
Fases 7-10 ya sean usables.

---

## Orden recomendado

**7 antes que todo lo demás** — es la causa raíz de "no puedo
seleccionar/reordenar contenedores"; sin esto, Fase 8 (borrar) tiene
el mismo problema (no hay nodo para borrar si no se puede seleccionar)
y Fase 9 (feedback de drag) mejora una interacción que hoy ni arranca
para la mayoría de los contenedores.

Después, 8 y 9 son independientes entre sí (se pueden hacer en
cualquier orden o en paralelo). 10 es polish, se puede ir intercalando.
11 queda para cuando el resto esté estable.

**Estado al cierre de esta sesión:** 7, 8, 9, 10 y 10.1 están
implementados, sin compilar. Pendiente real para continuar:

1. **Compilar y probar** — nada de lo de arriba pasó por un build
   real todavía; hay dos puntos sensibles a revisar primero:
   - El desvío de Fase 10 (`BeginDisabled(true)` en `DrawRealWidget`,
     ver esa sección) — confirmar que seleccionar/arrastrar sigue
     andando igual con los widgets reales puestos, y que ningún
     widget "roba" un click.
   - Fase 10.1 (esta sesión) — confirmar que no queda ninguna
     referencia colgante a la API de Preview removida
     (`SetDesignerPreviewActive` y compañía, `PreviewLogLine`) en
     algún caller no revisado, y que `editor_panel.cpp` compila con
     el bloque de la Design view reescrito (canvas usando
     `GetContentRegionAvail()` completo, sin la barra/consola).
2. **Fase 11** (undo/redo) — sigue sin arrancar, opcional; único ítem
   que queda 🔲 en la tabla de progreso.
