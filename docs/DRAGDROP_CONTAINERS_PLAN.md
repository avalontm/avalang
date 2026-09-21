# Plan: Corrección de drag & drop en contenedores (Design)

Basado en inspección directa del código actual (no de docs previos). Diagnóstico
completo en la conversación previa; este documento solo ordena la corrección en
fases, una a la vez, con zip verificable después de cada una — mismo flujo que
`CONTROLS.md`/`PROPERTY_METADATA_PLAN.md`.

Archivos involucrados:
- `runtime/avastudio/src/design/design_document.cpp` — `MoveNode` (el bug real).
- `runtime/avastudio/src/designer/drop_target.cpp` — `ComputeDropZone`/`ComputeDropIndicator`.
- `runtime/avastudio/src/panels/designer_canvas.cpp` — `HandleDropTarget`, `ComputeDropZone` (wrapper), `DrawDropIndicator`.
- `runtime/avaui/src/components/Component.cpp` — `AddChild`/`RemoveChild` (solo si Fase 3 requiere una variante posicional).

## Fase 0 — Arnés de verificación (sin tocar lógica) — COMPLETADA

Se adoptó como prueba formal en vez de ejecutable descartable, siguiendo la
convención existente de `tests/unit/avastudio/` bajo `AVA_BUILD_STUDIO_TESTS`.

- Archivo: `tests/unit/avastudio/MoveNodeOrderTest.cpp`.
- Target CMake: `ava_studio_move_node_order_test` (`runtime/avastudio/CMakeLists.txt`).
- No depende de ImGui ni OpenGL: solo `design_document.cpp`, `move_commands.cpp`
  y el árbol de componentes real de `avaui`.

Cobertura (111 checks tras la Fase 1; 101 en la línea base):
1. `MoveNode` con nodo externo (otro padre) en `kBefore`/`kAfter`: medio,
   primero, último y único hijo.
2. `MoveNode` reordenando dentro del mismo padre: adelante, atrás, extremos,
   no-ops (mover junto a su vecino) e intercambio de dos hijos.
3. `kInto`: agrega al final, mismo padre, contenedor vacío (invariantes que las
   fases 1 y 2 no deben romper).
4. Guardas: contenedor dentro de su propio descendiente, mismo nodo, raíz,
   destino inexistente.
5. Reordenar contenedores hermanos entre sí.
6. `MoveComponentCommand`: Execute/Undo/Redo/Undo con nodo del mismo padre, de
   otro padre, último hijo y primer hijo.
7. Añadidos en la Fase 1: la raíz como destino `kBefore`/`kAfter` no muta el
   árbol, y los slots no predeterminados se conservan.

Compilación local mínima (sin CMake, desde la raíz del repo; el script
`scripts/test_studio_dragdrop.sh` corre esta y las pruebas de las fases
siguientes):

```
g++ -std=c++20 -ffunction-sections -Wl,--gc-sections \
  -Iruntime/avaui/src -Iruntime/avastudio/src \
  tests/unit/avastudio/MoveNodeOrderTest.cpp \
  runtime/avastudio/src/design/design_document.cpp \
  runtime/avastudio/src/designer/move_commands.cpp \
  runtime/avaui/src/components/Component.cpp \
  runtime/avaui/src/components/ComponentTreeImpl.cpp \
  runtime/avaui/src/components/ComponentTree.cpp \
  runtime/avaui/src/components/PropertyValue.cpp \
  -o move_node_order_test
```

**Línea base (antes de la Fase 1): 77/101, 24 fallas** (10 checks añadidos después, ya en la Fase 1) — todas en el branch
`kBefore`/`kAfter` de `MoveNode` y en el Undo/Redo que lo reutiliza. Los grupos
`kInto` y guardas pasan completos.

Hallazgo que amplía el diagnóstico original: el defecto no solo desordena lo
que estaba después de `target`. Como `AddChild` es `push_back`, cualquier
`kBefore`/`kAfter` reubica al final a `target` y `moved` y deja todo lo demás
delante, incluso en no-ops (por ejemplo `A` antes de `B` en `A,B,C,D,E` produce
`C,D,E,A,B`). Solo queda correcto cuando `target` es el último hijo (`kBefore`)
o cuando `moved` termina naturalmente al final (`kAfter` sobre el último).

## Fase 1 — Arreglar `MoveNode` (bug de orden al reordenar) — COMPLETADA

**Alcance:** solo el branch `zone != kInto` de `MoveNode` en `design_document.cpp`.
Nada de UI, nada de `ComputeDropZone`.

Reemplazar el truco de "remove + 2×append" (que reubica a `target` y `moved`
juntos al final, corrompiendo todo lo que estaba después de `target`) por una
reconstrucción completa y estable del orden de hermanos:

1. Capturar `siblings = targetParent->Children()` (ya no incluye a `moved`,
   porque se desprendió de su padre anterior antes de este branch).
2. Construir la lista `ordered` recorriendo `siblings` e insertando `moved`
   justo antes o justo después de `target` según `zone`.
3. `RemoveChild` de todos los `siblings` actuales, luego `AddChild` en el orden
   de `ordered` (incluyendo a `moved` ya en su posición correcta).

Cubrir explícitamente en la implementación:
- Mover dentro del **mismo** contenedor (reordenar) vs. mover **desde otro**
  contenedor (reparent + posicionar).
- `target` es el único hijo, o ya es el primero/último (no debe regresar al
  comportamiento roto en estos bordes, pero tampoco debe tratarlos distinto).
- El **Undo** de `MoveComponentCommand` llama al mismo `MoveNode` — no requiere
  cambio de código propio, pero sí que quede cubierto por el arnés de la Fase 0
  (ya que hoy hereda el mismo bug).

**Implementado** (`design_document.cpp`, solo `MoveNode` y helpers locales):
- `InsertChildAt(parent, slot, child, index)` reconstruye el orden del slot:
  copia los hijos, los desprende, inserta `child` en `index` y los vuelve a
  agregar en orden. Recibe un índice (no un nodo `target`) para que la Fase 3
  lo reutilice tal cual.
- `SiblingInsertIndex` calcula el índice a partir de `target` y la zona
  (`kBefore` = posición de `target`, `kAfter` = posición + 1), sobre la lista ya
  sin `moved`. Esto unifica mismo-padre y otro-padre sin ramas separadas.
- `SlotOf` localiza el slot de `target`: la reconstrucción solo toca ese slot y
  no aplana los demás a `"default"`.
- `targetParent` se resuelve antes de desprender `moved`, por lo que el caso sin
  padre (raíz como destino) devuelve `false` sin mutar nada. Antes se
  desprendía y se reinsertaba al final de su padre original.
- `kInto` queda igual (`AddChild` al final).
- El Undo de `MoveComponentCommand` no requirió cambios de código.

**Resultado:** 111/111 checks en verde.

**No entra en esta fase:** el forzado de `kInto` sobre contenedores (Fase 2) ni
la inserción posicional dentro de un contenedor (Fase 3) — con esta fase sola,
reordenar hermanos que no sean contenedores ya queda bien, que es la mayoría de
los casos reales hoy (porque hoy nunca se llega a `kBefore`/`kAfter` contra un
contenedor, ver Fase 2).

**Verificación:** arnés de la Fase 0 en verde + prueba manual en pantalla
reordenando 4+ controles dentro de un `Column`/`Row` ya poblado.

## Fase 2 — Permitir `kBefore`/`kAfter` también sobre contenedores — COMPLETADA

**Alcance:** `ComputeDropZone` (`drop_target.cpp`) + `HandleDropTarget`
(`designer_canvas.cpp`).

Hoy: `if (isContainer) return DropZone::kInto;` sin excepción. Cambiar a una
regla por bandas verticales dentro del rect del contenedor, similar a la que ya
existe para no-contenedores pero con una banda central ancha para "into":

- Banda superior (p. ej. primer 20% de la altura) → `kBefore` (insertar como
  hermano antes del contenedor).
- Banda inferior (últimos 20%) → `kAfter` (hermano después).
- Banda central (60% restante) → `kInto` (comportamiento actual, sin cambios).

Esto se calcula en un solo lugar (`ComputeDropZone`) y automáticamente aplica
tanto al payload de Toolbox como al de mover un nodo existente, porque ambos ya
pasan por esta función — **excepto** el camino de Toolbox contra contenedor, que
hoy la bypassea del todo (`designer_canvas.cpp` líneas ~600-609, llama directo a
`InsertTool::InsertInto` sin calcular zona). Ese bypass hay que quitarlo y usar
el mismo cálculo de zona ahí también, ramificando a `InsertInto` (si zona =
into) o `InsertRelative` (si zona = before/after) igual que ya hace el branch de
no-contenedor.

`DrawDropIndicator` no necesita cambios: ya sabe dibujar tanto el rect completo
(into) como la línea (before/after) según la zona recibida — solo hay que
asegurarse de pasarle la zona real en vez de forzar `kInto`.

**Verificación:** en pantalla, arrastrar un control nuevo (o un contenedor
existente) y soltarlo cerca del borde superior/inferior de otro contenedor
hermano — debe insertarse como hermano, no anidarse. Soltar en el centro debe
seguir anidando como hoy.

**Implementado:**
- `ComputeDropZone(rect, point, isContainer, allowSibling)` (`drop_target.cpp`):
  para contenedores con `allowSibling`, banda superior → `kBefore`, inferior →
  `kAfter`, centro → `kInto`. Desviación respecto al 20% del plan: la banda es
  el 20% de la altura acotado a 6-16 px y nunca más del 30%, para que un
  contenedor alto (por ejemplo la página completa) no convierta una porción
  enorme en zona de hermano, y uno chico conserve zona central. Un contenedor
  sin `allowSibling` (la raíz, que no tiene padre) siempre resuelve `kInto`.
  El parámetro es obligatorio para que ningún llamador herede el cambio sin
  decidirlo.
- `HandleDropTarget` (`designer_canvas.cpp`): se eliminó el bypass del Toolbox
  contra contenedores. Ahora el payload del Toolbox y el de mover un nodo usan
  la misma zona; el Toolbox ramifica a `InsertInto` (`kInto`) o
  `InsertRelative` (`kBefore`/`kAfter`). El indicador recibe la zona real.
- Geometría: con el encabezado de contenedor activo había dos llamadas a
  `HandleDropTarget` con sub-rectángulos (encabezado y cuerpo), lo que habría
  calculado las bandas sobre un rectángulo parcial. Ambas llamadas reciben ahora
  el rectángulo completo del nodo, y se eliminó `hit_p1`, que quedó sin uso. Una
  consecuencia visible: el resaltado `kInto` rodea todo el contenedor y no solo
  el encabezado o el cuerpo.
- `CanDropAsSibling`: la raíz nunca puede ser hermano de nada.
- `document_tree_panel.cpp` pasa `allowSibling = false`: el árbol del documento
  conserva su comportamiento actual (un contenedor siempre resuelve `kInto`).
  Con nodos expandidos, un `kAfter` en la fila del contenedor se dibuja pegado
  a la fila pero insertaría después de todo su subárbol; queda fuera de este
  plan.

**Pruebas:** `tests/unit/avastudio/DropZoneTest.cpp`, target
`ava_studio_drop_zone_test` (38 checks: contenedor alto, de 56 px, de 20 px y de
10 px, fuera de rango, raíz, hoja, rectángulo desplazado e indicadores).
`ComputeDropZone` es una función pura, así que la regla queda verificada sin
ImGui. `HandleDropTarget` depende de ImGui y no se ejecutó aquí; solo se
comprobó que compila y no genera advertencias contra una versión mínima de
ImGui.

**Verificación en pantalla pendiente:** arrastrar un control nuevo (o un
contenedor existente) cerca del borde superior/inferior de otro contenedor
hermano debe insertarlo como hermano; el centro debe seguir anidando; soltar
sobre la raíz siempre anida.

## Fase 3 — Inserción posicional dentro de un contenedor (`kInto`) — COMPLETADA

**Alcance:** dónde cae el nuevo/movido hijo dentro de los hijos existentes del
contenedor, en vez de siempre al final (`push_back`).

1. Calcular, con los rects ya cacheados de los hijos actuales del contenedor
   (`uid_to_rect`), en qué índice cae el punto de suelta (mismo tipo de cálculo
   de "antes/después" que la Fase 2, pero contra cada hijo en vez de contra el
   contenedor completo).
2. Generalizar el helper de reconstrucción de orden de la Fase 1 (recibir un
   índice de destino en vez de un nodo `target`) para reutilizarlo aquí y evitar
   dos implementaciones del mismo patrón de "remove todos + re-add en orden".
3. Para contenedores sin hijos (caso trivial) o cuando el punto de suelta cae
   después del último hijo, el comportamiento debe coincidir con el actual
   (`push_back` al final) — no debería haber regresión ahí.

Nota aparte para `Grid`: con esto, soltar sobre una celda intermedia insertará
en esa posición del arreglo de hijos, pero el mapeo posición→celda visual de
`Grid` es automático por índice (`columns`/`rows`), así que el resultado ya será
"cae en esa celda" sin lógica adicional específica de Grid.

**Verificación:** en pantalla, insertar un control en medio de un `Row`/`Column`
ya poblado y confirmar que cae exactamente donde se soltó, no al final; repetir
en un `Grid` de 2×2 y confirmar que ocupa la celda esperada.

**Implementado:**
- Decisión de diseño distinta a la del punto 2: en vez de generalizar un helper
  para recibir un índice, se tradujo la posición a "antes del hijo `anchor`"
  (`kBefore`). `InsertChildAt` ya recibía un índice desde la Fase 1 y
  `MoveNode` con `kBefore` lo usa, así que no hubo que tocar `design_document`,
  `MoveComponentCommand` (ni su Undo) ni `InsertTool`. Si el punto cae después
  del último hijo, el destino es el contenedor con `kInto` (agrega al final,
  igual que antes).
- `drop_target.cpp` (puro, sin ImGui): `FlowLayoutOf` decide el eje por tipo
  (Row horizontal; Column/For/If vertical; ScrollView/ListView/Flex según
  `direction`; Grid por `columns`; Page/Container/Stack se apilan y siempre
  agregan al final). `ComputeInsertIndex` devuelve el índice: en flujo
  vertical/horizontal, antes del primer hijo cuyo centro queda después del
  punto; en `Grid`, primero elige la fila (por el borde inferior de sus celdas
  y no por una rejilla fija, así tolera celdas alineadas al centro) y luego la
  columna. Un `Grid` de una sola columna se trata como vertical.
  `ResolveInsertPosition` salta el nodo que se está moviendo, para que soltarlo
  sobre sí mismo o justo después no cambie nada. `ComputeInsertMarker` calcula
  la línea de inserción.
- `designer_canvas.cpp`: `ResolveDrop` reúne los rectángulos de los hijos
  desde `uid_to_rect` más el origen y el desplazamiento del encabezado (los
  mismos que usa `DrawNode` al dibujarlos; el margen por profundidad es
  simétrico y no mueve el centro), excluye los `Dialog` (no se dibujan) y
  devuelve zona, ancla y marcador. `HandleDropTarget` usa esa resolución para
  el indicador (el contorno del contenedor más una línea en el punto de
  inserción) y para ambos payloads.
- `tools.cpp`: `InsertRelative` ahora agrupa "crear" y "mover" en una
  transacción de `CommandManager`, porque con esta fase es el camino normal de
  un drop del Toolbox en un contenedor con hijos y dejaba dos pasos de Undo
  (el primer Ctrl+Z solo movía el control al final). Si ya hay una transacción
  abierta no la cierra.

**Pruebas:**
- `DropZoneTest.cpp` (`ava_studio_drop_zone_test`): 102 checks en total
  (eje por tipo, índice vertical/horizontal/grid, bordes, posición con nodo
  movido y marcadores).
- `InsertRelativeUndoTest.cpp` (`ava_studio_insert_relative_undo_test`): 15
  checks; falla 6 de 15 con la versión anterior de `InsertRelative`. Para
  compilarla sin CMake hay que definir `AVA_STUDIO_TEST_STUB_PARSER`, que
  aporta `CanonicalTypeName` en lugar de `avalang_ui`, y enlazar con
  `-ffunction-sections -Wl,--gc-sections`.
- `HandleDropTarget`/`ResolveDrop` dependen de ImGui y no se ejecutaron; se
  comprobó compilación y ausencia de advertencias contra una versión mínima de
  ImGui. El target de CMake de `InsertRelativeUndoTest` se armó a partir del
  cierre de dependencias que resolvió `g++`, sin ejecutar CMake.

**Límites conocidos:**
- Page, Container y Stack apilan a sus hijos (así los distribuye el motor de
  layout), por lo que no tienen un "entre dos hijos" y siguen agregando al
  final.
- Un `Dialog` dentro de un `Grid` ocupa una celda en el motor de layout pero no
  se dibuja; se excluye del cálculo de posición.
- En `Grid`, insertar en una celda intermedia desplaza a los siguientes hijos
  una celda (el orden del arreglo define la celda).

**Verificación en pantalla pendiente:** insertar un control en medio de un
`Row`/`Column` ya poblado y confirmar que cae donde se soltó y que se ve la
línea de inserción; repetir en un `Grid` de 2×2; arrastrar un control existente
dentro de su propio contenedor; y confirmar que un solo Ctrl+Z revierte la
inserción completa.

## Fase 4 — Documentación y cierre — COMPLETADA

**Qué se corrigió**
- `MoveNode` ya no desordena los hermanos al reordenar (Fase 1). Esto también
  arregla el Undo de mover y el `InsertRelative` que usa el Toolbox.
- Soltar cerca del borde superior o inferior de un contenedor lo inserta como
  hermano; el centro sigue anidando; la raíz siempre anida (Fase 2).
- Soltar dentro de un contenedor con hijos inserta en la posición del cursor
  en vez de al final, con una línea de inserción (Fase 3).
- Un drop del Toolbox en medio de un contenedor se revierte con un solo
  Ctrl+Z (Fase 3, `InsertRelative` transaccional).

**Invariantes garantizadas** (cubiertas por las pruebas)
1. `MoveNode(kBefore/kAfter)` deja el nodo inmediatamente antes o después del
   destino y conserva el orden relativo de todos los demás hermanos, tanto al
   reordenar dentro del mismo padre como al reparentar.
2. Un movimiento que no es válido (mismo nodo, raíz, destino dentro del propio
   nodo, destino inexistente o raíz como hermano) devuelve `false` y no muta el
   árbol.
3. `MoveNode` solo reconstruye el slot del destino; los demás slots no cambian.
4. `MoveComponentCommand` hace Execute → Undo → Redo → Undo sin alterar el
   orden original.
5. `ComputeDropZone` nunca devuelve `kBefore`/`kAfter` para un contenedor si
   `allowSibling` es falso, y siempre deja una zona central para `kInto`.
6. La posición de inserción nunca cae sobre el propio nodo movido.
7. `InsertRelative` con un `CommandManager` deja exactamente un paso de Undo y
   no cierra una transacción que ya estuviera abierta.

**Fuera de alcance a propósito**
- Árbol del documento (`document_tree_panel.cpp`): sus contenedores siguen
  resolviendo siempre `kInto` (`allowSibling = false`).
- Page, Container y Stack apilan a sus hijos: no hay "entre dos hijos", se agrega
  al final.
- Un `Dialog` dentro de un `Grid` ocupa una celda en el layout pero se excluye
  del cálculo de posición.
- `scrollOffsetX/Y` de ScrollView/ListView no se considera en el cálculo (no hay
  scroll interactivo en Design).
- `HandleDropTarget`/`ResolveDrop` dependen de ImGui y no tienen prueba
  automatizada; solo la parte pura (`drop_target.cpp`) la tiene.

**Pruebas y arnés (cierre de la Fase 0)**

El arnés dejó de ser descartable: quedó como pruebas formales en
`tests/unit/avastudio/`, registradas bajo `AVA_BUILD_STUDIO_TESTS`.

| Prueba | Target CMake | Checks |
|---|---|---:|
| `MoveNodeOrderTest.cpp` | `ava_studio_move_node_order_test` | 111 |
| `DropZoneTest.cpp` | `ava_studio_drop_zone_test` | 102 |
| `InsertRelativeUndoTest.cpp` | `ava_studio_insert_relative_undo_test` | 15 |

Ejecución sin CMake y sin ImGui/OpenGL (solo `g++`):

```
bash scripts/test_studio_dragdrop.sh
```

Compila las tres pruebas con las fuentes mínimas y muestra el resumen de cada
una; si alguna falla, imprime solo las líneas que fallaron y termina con código
distinto de cero. Los targets de CMake de las Fases 2 y 3 se armaron sin
ejecutar CMake; conviene compilarlos una vez con `AVA_BUILD_STUDIO_TESTS=ON`.

**Checklist de verificación en pantalla (pendiente)**
1. Reordenar 4+ controles dentro de un `Column`/`Row` ya poblado, con Ctrl+Z y
   Ctrl+Y en cada movimiento.
2. Soltar un control del Toolbox (y arrastrar un contenedor existente) cerca del
   borde superior/inferior de un contenedor hermano: debe quedar como hermano.
3. Soltar en el centro de un contenedor: debe anidar. Soltar sobre la raíz:
   siempre anida.
4. Insertar en medio de un `Row`/`Column` poblado: cae donde se soltó y se ve la
   línea de inserción. Repetir en un `Grid` de 2×2.
5. Arrastrar un control existente dentro de su propio contenedor.
6. Un solo Ctrl+Z revierte por completo un drop del Toolbox.

---
_Orden de fases pensado por riesgo/alcance: Fase 1 es la corrección de un bug
real, aislada a una función, sin tocar UI — la de menor riesgo y mayor impacto,
por eso va primero. Fase 2 cambia comportamiento visible (qué pasa al soltar
cerca del borde de un contenedor) — riesgo medio. Fase 3 es la de mayor alcance
(nueva capacidad, no solo corrección) — va última._
