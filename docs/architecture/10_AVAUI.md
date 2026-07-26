# AvaUI — Arquitectura General

## 0. Qué es este documento

Describe el estado **real** del código hoy, no la visión. La visión
(hacia dónde apunta AvaUI, incluyendo piezas que no existen todavía
como un backend HTML5/CSS) vive por separado en
`AVAUI_FRAMEWORK.md` -- este documento es la referencia a usar
cuando la pregunta es "¿esto ya existe o es roadmap?".

Es parte de una serie de documentos de arquitectura de AvaUI; el mapa
completo (incluyendo qué piezas de la serie todavía no están escritas)
está en `docs/architecture/00_SYSTEM_ARCHITECTURE.md`. La sección 7 de
este doc indexa los documentos de la serie que sí existen hoy.

## 1. El problema: dos AvaUI hoy

Hay dos implementaciones del mismo concepto (un árbol de componentes de
UI + su serialización `.avaui`) que todavía no convergen:

| | `core/src/ui/` | `studio/src/design/` |
|---|---|---|
| Tipo del nodo | `ava::ui::Component` (`component.h`) | `DesignNode` (`design_document.h`) |
| Para quién | Cualquier host vía C API (`avalang.h`, `ava_ui_*`) | Solo Ava Studio, en memoria |
| Parser/writer `.avaui` | `avaui_text.{h,cpp}` -- **el canónico**, pensado para todo host | `avaui_text.{h,cpp}` propio (nombre igual, archivo distinto) + `avaui_json.cpp` |
| Identidad de nodo | Ninguna estable más allá del puntero `Component*` | `node_uid` (generado en memoria, ver `GenerateNodeUid()`) |
| Campos "de editor" | No tiene -- es un árbol runtime puro | `node_uid`, y el documento dueño (`DesignDocument`) trae `selected_uid`, `dirty`, `code_behind`, `initial_state`, `imports` |
| Layout (geometría real) | No calcula nada (`layout.h` solo mapea `LayoutType` ↔ nombre) | `layout_engine.cpp` calcula rects reales (`ComputeLayout`) |
| Render | No aplica (no es su capa) | `designer_canvas.cpp` (ImGui) |

El propio comentario de `core/src/ui/avaui_text.h` es explícito sobre la
intención:

> `studio/src/design/avaui_text.{h,cpp}` (DesignNode-based) [...] son las
> dos implementaciones existentes que esto está pensado para eventualmente
> obsoletar.

Es decir: **la dirección ya está decidida para el parser** (converger en
`core`). Lo que este documento resuelve es qué pasa con todo lo demás
que hoy solo existe del lado de Studio: layout, render, lifecycle.

## 2. Por qué existen dos (no es solo deuda técnica)

No es un accidente que `DesignNode` tenga más campos que `Component`:

- **`core::Component`** tiene que ser una ABI estable y mínima, cruzada
  por la frontera C (`avalang.h`) hacia hosts que ni siquiera son C++
  (el binding .NET, potencialmente Python/Rust/Godot más adelante). Cada
  campo que se le agrega es superficie de API que hay que mantener para
  siempre.
- **`DesignNode`** vive enteramente del lado C++ del Designer y necesita
  cosas que un runtime headless correría innecesariamente: selección
  actual, flag de "sucio" para el asterisco del tab, un uid estable para
  hit-testing que sobrevive a que el usuario edite `id` a mano.

O sea que un poco de separación es correcta a propósito. El problema real
no es que existan dos structs -- es que **el layout y el render también
quedaron atados a `DesignNode`**, cuando ninguno de los dos debería
depender de campos que son puramente de editor. Eso es lo que bloquea a
un futuro host no-Studio (o incluso a un modo "Preview sin editar" dentro
del propio Studio) de reusar ese trabajo.

## 3. Capas (diagrama actualizado, no aspiracional)

```
┌──────────────────────────────────────────────────────────────────┐
│ Host application (Ava Studio hoy; futuro: app .NET, juego, etc.) │
├──────────────────────────────────────────────────────────────────┤
│ EngineBridge / C API -- ava_compile() / ava_run() / ava_ui_*()   │
├──────────────────────────────────────────────────────────────────┤
│ AvaLang Core -- VM + Compiler (implementado, estable)            │
├──────────────────────────────────────────────────────────────────┤
│ AvaUI Core -- ava::ui::Component tree + parser/writer .avaui     │
│ (implementado: core/src/ui/component.*, avaui_text.*)           │
├──────────────────────────────────────────────────────────────────┤
│ Layout engine -- HOY solo existe del lado Studio (ver sección 5) │
├──────────────────────────────────────────────────────────────────┤
│ Render pipeline -- HOY solo hay un backend: ImGui/Studio         │
│ (designer_canvas.cpp, documentado en 16_STUDIO.md; un doc propio  │
│  de render pipeline todavía no está escrito, ver sección 7)      │
└──────────────────────────────────────────────────────────────────┘
```

Ava Studio no es un cliente cualquiera de este stack: hoy vive *al lado*
de la capa "AvaUI Core", con su propio modelo (`DesignNode`) y su propio
parser, no *encima* de ella. `component_resolver.cpp` es el único puente
real que existe hoy -- resuelve `import`/`Componente()` armando un árbol
de `DesignNode` "sintético" a partir de otro `.avaui`, pero sigue
quedándose en `DesignNode`, nunca cruza a `Component`.

## 4. Qué existe hoy vs qué es roadmap

| Pieza | Estado | Dónde |
|---|---|---|
| VM + compiler + C API | Implementado, estable | `core/src/vm`, `core/src/compiler`, `public/src/c_api.cpp` |
| `Component` tree runtime | Implementado | `core/src/ui/component.*` |
| Parser/writer `.avaui` canónico | Implementado | `core/src/ui/avaui_text.*` |
| `DesignNode` (mirror de editor) | Implementado, a converger (sección 6) | `studio/src/design/design_document.*` |
| Resolución de `import`/`Componente()` | Implementado (solo entre `DesignNode`) | `studio/src/design/component_resolver.cpp` |
| Layout engine (geometría real) | Implementado, **solo en Studio**, sobre `DesignNode` | `studio/src/design/layout_engine.*` |
| Render pipeline | Implementado, **un solo backend** (ImGui/Studio, de solo-editor) | `studio/src/panels/designer_canvas.cpp` |
| Widget lifecycle formal (mount/update/unmount) | No existe como concepto propio, lógica dispersa | `designer_canvas.cpp` (clicks/eventos) + `state_eval.cpp` (evaluación de props) |
| Backend HTML5/CSS | Roadmap, 0% | -- |
| Backend nativo (controles del SO) | Roadmap, 0% | -- |
| Runtime headless (correr un `.avaui` sin Studio) | Roadmap, 0% -- ni siquiera hay quién ejecute el `click` de un botón fuera del Designer | -- |

## 5. Decisión: dónde vive el layout engine

Esta era la pregunta abierta antes de escribir esta serie de docs.
Resolución:

**El algoritmo se promueve a `core/src/ui/` y opera sobre `Component`,
no sobre `DesignNode`.** Detalle completo, con el diseño del adaptador,
en `12_LAYOUT.md`. Razonamiento corto acá:

- `layout_engine.cpp` **ya es agnóstico de ImGui hoy** -- `Rect` es un
  struct propio (`x/y/w/h` en `float`), no `ImVec2`/`ImRect`. El único
  motivo por el que no es agnóstico de *Studio* es que su firma toma un
  `const DesignNode&` en vez de un `const Component&`. Es un cambio de
  tipo de parámetro, no una reescritura.
- La dirección ya elegida para el parser (converger en `core`, ver
  sección 1) sienta el precedente: cualquier pieza que no dependa
  genuinamente de estado de editor (`node_uid`, `selected_uid`, `dirty`)
  debería vivir en `core` por default, y quedarse en Studio solo si hay
  una razón concreta.
- Un futuro backend no-Studio (headless, HTML5, lo que sea) necesita
  layout real para poder dibujar algo -- si el algoritmo se queda atado
  a `DesignNode`, ese backend tiene que reimplementarlo o Studio tiene
  que exportarle `DesignNode` igual, lo cual reintroduce el acoplamiento
  que se supone estamos rompiendo.

**Trade-offs, documentados en vez de ignorados:**

| | A favor de promover a `core` | En contra |
|---|---|---|
| Fuente única de verdad | Un solo algoritmo, un solo lugar con tests (`core/tests`) | Studio necesita un paso de conversión `DesignNode → Component` antes de layoutear |
| Testeable sin ImGui | Ya lo es (`Rect` es plano) -- se vuelve testeable también sin Studio | -- |
| Backends futuros lo heredan gratis | Sí, ese es el punto | El algoritmo hoy tiene decisiones hardcodeadas de Studio (`kDefaultLeafHeight` fijo, sin medir texto real) que hay que sacar de ahí también, no alcanza con mover el archivo |
| Costo en el hot path del Designer (corre cada frame) | Se puede cachear igual que ya se cachea `resolved_root` (ver `component_resolver.cpp`/`docs/history/DESIGNER_VIEW_SESSIONS.md` 9.16) | Igual es trabajo extra a hacer, no es gratis |

**Mitigación concreta para el punto de "texto real"**: en vez de que
`ComputeLayout` sepa medir texto (eso depende 100% del backend -- ImGui
mide distinto que un navegador), la firma debería aceptar una interfaz
de medición inyectada (`LeafMeasurer` o similar, un `std::function`
alcanza para empezar) en vez de la constante fija
`kDefaultLeafHeight` que usa hoy. El backend ImGui le pasa
`ImGui::CalcTextSize`; un futuro backend le pasa lo que corresponda.
Detalle de la firma propuesta en `12_LAYOUT.md`.

## 6. Plan de convergencia (fases, alto nivel)

No es para hacer todo de una -- es para que cada fix futuro sepa hacia
dónde empuja.

- **Fase A (ya hecha)**: `core/src/ui/avaui_text.*` existe y está
  documentado como el parser canónico destinado a reemplazar a los dos
  duplicados.
- **Fase B**: extraer `ComputeLayout` a `core/src/ui/layout.cpp`,
  parametrizado sobre `Component` + un `LeafMeasurer` inyectado (sección
  5). Studio pasa a convertir `DesignNode → Component` (un pase, igual
  de barato que la resolución de imports que ya hace) antes de
  layoutear, y cachea el resultado igual que hoy.
- **Fase C**: evaluar si `DesignNode` puede reducirse a una capa fina de
  metadata de editor *sobre* `Component` (uid, selección) en vez de una
  struct paralela completa que duplica `type`/`id`/`properties`/`events`/
  `children`. No es obligatorio para que Fase B funcione -- es una
  limpieza posterior.
- **Fase D**: separar el render pipeline de Studio en "generar comandos
  de dibujo genéricos" vs "ImGui pinta esos comandos" (el contrato para
  esto todavía no tiene doc propio -- ver sección 7, es uno de los
  huecos pendientes).
- **Fase E**: primer backend real no-Studio. Candidato más barato para
  validar el contrato: uno headless/de test (produce una lista de
  comandos y nada más, sin ventana), no necesariamente HTML5 -- HTML5
  es mucho más trabajo (DOM real, CSS, eventos del browser) para ser el
  primer test del contrato.

## 7. Relación con los demás documentos de esta serie

Documentos que existen hoy:

- **`11_COMPONENT_TREE.md`** -- `Component`/`ComponentTree`/`LayoutType`
  y la C API completa.
- **`12_LAYOUT.md`** -- el algoritmo de layout en detalle
  (Column/Row/Stack/Grid/Flex), la firma final con `LeafMeasurer`, y el
  adaptador `DesignNode → Component`.
- **`16_STUDIO.md`** -- Ava Studio en sí: paneles, `DesignDocument`,
  tabs, los tres árboles de UI que conviven en memoria, y su relación
  (y deuda) con `core/src/ui`.
- **`17_AVAUI_FILE_FORMAT.md`** -- el formato de texto `.avaui`
  (`state`/`view`/`methods`).

Documentos que esta serie referencia pero que **todavía no están
escritos** (huecos reales, no perdidos en la reorganización -- ver
`00_SYSTEM_ARCHITECTURE.md`):

- **Render pipeline** -- el pipeline de `designer_canvas.cpp`
  documentado como implementación de referencia del backend #1, con la
  línea trazada entre qué es genérico y qué es ImGui-específico.
- **Widget lifecycle** -- mount/update/unmount, cuándo se re-evalúa una
  prop, cómo se conectan eventos -- formalizando lo que hoy vive
  disperso en `designer_canvas.cpp` + `state_eval.cpp`.
- **Render backends (contrato)** -- el contrato que un backend debe
  cumplir (construido sobre layout+render pipeline+widget lifecycle),
  ImGui documentado como backend #1 implementado, el resto como
  roadmap explícito.

## 8. Preguntas abiertas (para retomar en su momento)

- ¿`DesignNode` termina desapareciendo del todo (Fase C) o siempre va a
  necesitar existir como struct completa por alguna razón que todavía
  no vimos (ej. representar estados intermedios/inválidos que
  `Component` no debería poder representar nunca)?
- ¿`node_uid` (o un equivalente) termina viviendo también en `Component`
  para que un futuro backend interactivo (no solo Studio) tenga una
  identidad estable de nodo para hit-testing, o eso queda
  permanentemente como concepto solo-de-editor?
- Una vez que exista un host no-Studio ejecutando un `.avaui` de verdad:
  ¿quién es dueño del ciclo de vida del árbol `Component` en runtime
  (GC del lado del host, ownership por `shared_ptr` como ya es hoy,
  algo más explícito)? Hoy nunca se ejercitó ese camino end-to-end.
