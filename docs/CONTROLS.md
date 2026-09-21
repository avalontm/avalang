# Soporte de controles en AvaStudio (render en Design + metadatos de Properties)

Dos frentes de trabajo sobre los mismos controles, en el mismo documento para no
duplicar la tabla:

1. **Render en el canvas Design** — `avastudio/src/panels/designer_canvas.cpp`,
   función `DrawRealWidget`. Cada control depende de una rama `type == "..."`
   que dibuja con primitivas de ImGui/`ImDrawList` directamente; si no tiene
   rama, cae al fallback genérico (nombre del tipo + valor evaluado como texto
   plano) — sigue siendo seleccionable/movible, solo no se ve como el control
   real (aunque sí funcione en Preview/HTML, porque `HTMLRenderer` y
   `GdiRenderer` ya los implementan). Esta nota reemplaza la referencia previa a
   `imgui_renderer.{h,cpp}`/`OnDraw*` (`BaseRenderer`), que ya no es el
   mecanismo vigente.
2. **Metadatos dinámicos del panel Properties** — cada control declara sus
   propiedades en su `RegisterComponentType(...)` (`avaui/src/controls/*.cpp`).
   Con metadata explícita (`PropertyKind`/`PropertyGroup`, ver
   `PROPERTY_METADATA_PLAN.md`) el panel muestra el editor correcto (texto,
   número, booleano, color, enum, recurso) y la categoría correcta, en vez de
   adivinar por el nombre de la propiedad.

| # | Control (type) | Categoría | Render en Design | Metadata en Properties |
|---|-----------------|-----------|:---:|:---:|
| 1 | `page` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 2 | `container` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 3 | `column` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 4 | `row` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 5 | `stack` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 6 | `grid` | Layout | ✅ | ✅ (`columns`, `rows`) |
| 7 | `flex` | Layout | ✅ | ➖ (sin propiedades registradas) |
| 8 | `text` (Label) | Content | ✅ | ✅ (`text`) |
| 9 | `image` | Content | ✅ | ✅ (`source`, `alt`) |
| 10 | `link` | Content | ✅ | ✅ (`text`, `href`) |
| 11 | `button` | Interactive | ✅ | ✅ (`text`, `isEnabled`, `style`) |
| 12 | `textbox` | Interactive | ✅ | ✅ (8 propiedades, ver plan) |
| 13 | `checkbox` | Interactive | ✅ | ✅ (`label`, `isChecked`, `isEnabled`) |
| 14 | `radiobutton` | Interactive | ✅ | ✅ (`label`, `group`, `isSelected`, `isEnabled`) |
| 15 | `combobox` | Controles | ✅ | ⬜ (`selectedValue`, `isEnabled`, `isOpen`) |
| 16 | `icon` | Controles | ✅ | ⬜ (`source`) |
| 17 | `dialog` | Layout | ➖¹ | ⬜ (`title`, `isOpen`, `dismissible`) |
| 18 | `scrollview` | Layout | ✅ | ⬜ (`scrollOffsetX`, `scrollOffsetY`) |
| 19 | `listview` | Layout | ✅ | ⬜ (`scrollOffsetX`, `scrollOffsetY`) |

> **Corrección respecto a la versión anterior de este documento:** el catálogo
> del Toolbox (`ComponentRegistry` en `avastudio/src/designer/component_registry.cpp`)
> recorre **todo** `avalang::ui::registry::GetComponentTypeRegistry()`, no el CSV
> `avastudio/data/design/component_catalog.csv`. El CSV solo aporta `order`/
> `category`/`icon` para los tipos que lista (1-14); los que no están en el CSV
> (15-19) igual aparecen en el Toolbox, arrastrables, solo que ordenados al
> final bajo una categoría genérica (`Layout` o `Controles`). Es decir: los 19
> tipos registrados en el runtime siempre fueron arrastrables — nunca fue un
> problema del catálogo.
>
> ¹ `dialog` se crea igual que cualquier otro control al soltarlo, pero
> `designer_canvas.cpp` lo excluye a propósito del árbol dibujado en el canvas
> (`IsDialogNode` → `continue`) y lo muestra en la bandeja "Diálogos" aparte
> (`DrawDialogTray`). No es un bug: es la razón por la que "no se ve nada" al
> soltar `dialog` en el lienzo.
>
> **Soltar en contenedores** (orden de hermanos, `kBefore`/`kAfter` sobre
> contenedores, inserción posicional): corregido en el Design; el detalle, las
> invariantes y lo que quedó fuera están en `DRAGDROP_CONTAINERS_PLAN.md`.

## Leyenda
- ✅ Implementado / migrado.
- ⬜ Pendiente.
- ➖ No aplica (el control no tiene propiedades registradas hoy, o en el caso
  de `dialog`, no se dibuja en el canvas por diseño — ver nota ¹).

## Convención de implementación — Render en Design
Cada control se resuelve uno por uno siguiendo el patrón ya usado por `GdiRenderer`
(referencia de dibujo nativo por primitivas) y `HTMLRenderer` (referencia de
comportamiento/props), adaptado a primitivas de `ImDrawList` (`AddRect`, `AddEllipse`,
`AddLine`, `AddText`, etc.) reutilizando las constantes de `layout/LayoutProperties.h`
para mantener consistencia visual entre Design, Preview y HTML.

## Convención de implementación — Metadata de Properties
Cada control declara sus propiedades reales en su `RegisterComponentType(...)`
usando `PropertyDescriptor` con `kind`/`group`/`description` explícitos (ver
detalle completo, decisiones de diseño y fases en `PROPERTY_METADATA_PLAN.md`).

---
_Última actualización: render en Design 18/19 completo (todo el Toolbox salvo
`dialog`, que por diseño no se dibuja en el canvas — ver nota ¹). Se agregó
render real para `combobox` (input de solo lectura + flecha) e `icon` (imagen
si tiene `source` válido, si no un glifo circular genérico, distinto del
placeholder de `image`). Metadata de Properties sigue en 14/19 migrados —
`combobox`, `icon`, `dialog`, `scrollview`, `listview` siguen pendientes (⬜),
rastreados en `PROPERTY_METADATA_PLAN.md`._

