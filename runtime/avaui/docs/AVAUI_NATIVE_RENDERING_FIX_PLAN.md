# AVAUI — Plan de corrección: render 100% nativo (sin CSS/Tailwind)

**Ubicación sugerida en el repo:** `runtime/avaui/docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md`
(mismo lugar donde viven los demás `AVAUI_FASE*`).

## 0. Contexto (resumen de la sesión anterior)

Decisión ya tomada y confirmada por el usuario: **el proyecto no va a usar
CSS/Tailwind**. Todo el UI (web, desktop, y a futuro mobile) se debe ver
igual porque lo calcula un único `LayoutEngine` nativo (row/column/fill/
padding/gap/width/height/align), y cada renderer (`GdiRenderer` en
desktop, `HTMLRenderer` en web) solo pinta las coordenadas en píxeles que
ese motor ya calculó. Esa es la ventaja central del framework frente a
usar HTML+CSS normal.

Se diagnosticó en la sesión pasada:

- `GdiRenderer::Draw*` **ignora por completo `className`** (ver
  `runtime/avaui/src/renderer/GdiRenderer.cpp`) — cuando un elemento tiene
  `class=`, `HTMLRenderer` deja que el navegador (Tailwind) resuelva su
  tamaño, pero `GdiRenderer` no sabe qué hacer con eso y usa el layout
  crudo. Esto es lo que hace que `class=` "rompa" desktop.
- El `LayoutEngine` (`runtime/avaui/src/layout/LayoutEngineImpl.cpp`,
  función `PlaceComponent`) **no tiene intrinsic/content-based sizing**
  (comentario explícito en el código, línea ~95): un elemento sin
  `width`/`height` y sin `align = stretch` recibe tamaño `0`; y con
  `align = stretch` (el default), se estira a llenar todo el espacio
  disponible del padre. Por eso los 3 botones de `routes/index.avaui`
  salían de 405×332px en vez de un tamaño natural de botón.

La decisión tomada fue: **no usar `class=` en ningún `.avaui` nuevo, y
resolver el intrinsic sizing para que los componentes nativos (botones,
texto) tengan un tamaño razonable sin tener que declarar `width`/`height`
a mano.**

## 1. Checklist de fases (marcar al completar)

> Ir marcando `[x]` a medida que se completa cada paso. No pasar a la
> fase siguiente sin cerrar la anterior (ver orden y dependencias en la
> sección 2).

### Fase A — Intrinsic sizing (motor de layout) — ✅ COMPLETADA

- [x] Crear `runtime/avaui/src/layout/TextMeasure.h` + `.cpp` con la
      función de medición de texto (heurística propia, sin depender de GDI)
- [x] Agregar `IntrinsicSize` + `ComputeIntrinsicSize` en
      `runtime/avaui/src/layout/LayoutEngineImpl.h`
- [x] Implementar el measure pass (bottom-up) en
      `runtime/avaui/src/layout/LayoutEngineImpl.cpp` (llamado desde
      `Compute()` antes del pase de posicionamiento) y reemplazar el
      `width = 0.0` / `height = 0.0` de `PlaceComponent` por el
      resultado cacheado del measure (`intrinsic_`)
- [x] Corregido también `ArrangeRowOrColumn`: los hijos "auto" (sin
      `width`/`height` explícito) ahora usan su tamaño intrínseco en el
      eje principal en vez de repartirse el espacio sobrante en partes
      iguales — esta era la causa real de los botones de 405px de
      ancho en el ejemplo original
- [x] Agregar constantes de padding/tamaño por defecto de controles
      (botón) y fontSize de respaldo en
      `runtime/avaui/src/layout/LayoutProperties.h`
- [x] Actualizar el comentario desactualizado ("Phase 3 has no
      intrinsic/content-based sizing yet") en `LayoutProperties.h`
- [x] `runtime/avaui/src/layout/LayoutNode.h`/`.cpp`: revisado, no
      necesitó cambios — `Compute()` ya reconstruye el árbol completo
      en cada llamada, así que no hace falta invalidar un caché de
      tamaño intrínseco entre frames (se limpia junto con `nodes_` al
      inicio de cada `Compute()`)
- [x] Agregado `src/layout/TextMeasure.cpp` a
      `runtime/avaui/CMakeLists.txt`
- [x] Verificación de sintaxis (`g++ -fsyntax-only -Wall -Wextra`)
      sobre el módulo de layout modificado: sin errores ni warnings.
      **Pendiente para quien retome esto:** compilar el proyecto
      completo con el toolchain real (CMake + vcpkg, MSVC) y correr
      `render-static`/`render-dynamic` sobre el ejemplo de los 3
      botones para confirmar visualmente el tamaño resultante — esta
      sesión no tuvo acceso a ese toolchain (Windows-only) ni a
      AvaStudio para la comparación visual contra `GdiRenderer`.

### Fase B — Quitar Tailwind/CSS por defecto — ✅ COMPLETADA

- [x] `runtime/avahost/src/config/app_manifest.cpp` — `DefaultAppManifest()`
      devuelve manifest vacío (sin `css/app.css` ni Tailwind CDN)
- [x] Actualizar comentarios desactualizados en
      `runtime/avahost/src/config/app_manifest.h` (cabecera y
      `DefaultAppManifest()`)
- [x] `runtime/avahost/src/cli_commands.cpp` — `CmdNew`: quitar los
      `import` de Tailwind/`css/app.css` en el `app.ava` generado
- [x] `CmdNew` — reescribir `routes/index.avaui` generado sin `class=`
      (usar primitivas nativas: padding, gap, align)
- [x] `CmdNew` — reescribir `layouts/main.avaui` generado sin `class=`
- [x] `CmdNew` — actualizar/retirar el comentario de
      `wwwroot/css/app.css` que referencia Tailwind
- [ ] Verificar: `avahost new` + `avahost run` + `curl` no trae ningún
      `<script .../tailwindcss/...>` ni `<link ... css/app.css>` salvo
      que el usuario lo declare a mano. **Pendiente para quien retome
      esto:** esta sesión no tuvo acceso al toolchain real (CMake +
      vcpkg, MSVC, Windows-only) para compilar `avahost` y correr
      `new`/`run`/`curl` de punta a punta; solo se verificó sintaxis
      (`g++ -fsyntax-only`) de `app_manifest.cpp` de forma aislada —
      `cli_commands.cpp` no pudo compilarse ni siquiera en modo
      syntax-only fuera de Windows por dependencias de plataforma
      (`platform/interfaces/ILibrary.h` vía `plugin_loader.h`), mismo
      límite de entorno que Fase A.

### Fase C — El viewport no cubre toda la página — ✅ COMPLETADA

- [x] Decidir opción (1: centrar viewport fijo — recomendado para esta
      fase, o 2: viewport responsivo — fase aparte)
- [x] Modificar `runtime/avaui/src/renderer/HTMLRenderer.cpp` —
      `EmitHTMLHeader()`: centrar `.ava-viewport` y/o igualar fondo del
      `body` al del viewport
- [ ] Verificar con ventana de navegador más grande que 1280×720: sin
      recuadro flotando sobre fondo blanco. **Pendiente para quien
      retome esto:** sin toolchain real disponible en esta sesión
      (mismo límite que Fases A/B), solo se verificó sintaxis
      (`g++ -fsyntax-only`) de `HTMLRenderer.cpp` de forma aislada, sin
      errores. Falta compilar `avahost` completo, correr
      `render-static`/`render-dynamic` o `avahost run` y confirmar
      visualmente en un navegador con ventana >1280×720.

### Fase D — Documentar `class=` como no recomendado — ✅ COMPLETADA

- [x] Comentario en `runtime/avaui/src/renderer/HTMLRenderer.cpp`
      (ramas `hasClass` de `OnDrawRectangle`/`OnDrawEllipse`/`OnDrawText`)
- [x] Comentario en `runtime/avaui/src/renderer/GdiRenderer.cpp`
      (aclarando que ignora `className` y por qué no se recomienda usarlo)

---

## 2. Problemas a corregir en esta fase

Con el `curl http://localhost:8080` de esta sesión se confirman **tres
problemas separados**, no uno solo:

1. **El scaffold/manifest por defecto sigue cargando Tailwind y
   `css/app.css`**, aunque la decisión ya fue "no CSS". El HTML de salida
   trae `<script src=".../@tailwindcss/browser@4">` y
   `<link rel="stylesheet" href="/css/app.css">`, y `layouts/main.avaui`
   + `routes/index.avaui` generados por `avahost new` siguen usando
   `class = "flex ..."` en vez de las primitivas nativas. Esto hay que
   revertirlo — es la causa raíz de que sigan apareciendo `<div
   class="...">` vacíos (sin `ava-element`) mezclados con los que sí
   trae el motor nativo.
2. **Intrinsic sizing no implementado** (ver contexto arriba): sin esto,
   quitar `class=` del scaffold no alcanza — los botones seguirían
   gigantes salvo que cada `.avaui` declare `width`/`height` a mano en
   cada botón.
3. **El viewport (`.ava-viewport`, el recuadro gris de 1280×720) no cubre
   toda la página** — se ve el recuadro gris flotando y el resto de la
   página en blanco alrededor. Falta CSS de host (no de la app) que
   centre/ajuste el viewport dentro del `<body>`.

## 3. Orden recomendado de trabajo

Hacer **primero el punto A** (intrinsic sizing). Si se arregla primero el
scaffold (punto B) pero no el motor, cualquier `.avaui` nuevo sin
`width`/`height` explícito en botones seguirá saliendo gigante — solo
cambiaría el síntoma. El orden lógico es:

1. **A — Intrinsic sizing en el `LayoutEngine`** (arregla la causa raíz
   del tamaño).
2. **B — Dejar de inyectar Tailwind/CSS por defecto** y reescribir el
   scaffold de `avahost new` sin `class=`.
3. **C — Viewport no cubre la página completa** (arreglo de host,
   independiente de A y B, se puede hacer en paralelo).
4. **D — Decisión sobre el soporte de `class=`** que ya existe en los
   renderers (dejarlo como escape hatch documentado, o retirarlo).

---

## A. Intrinsic sizing (motor de layout)

**Objetivo:** que un componente sin `width`/`height` explícito (ej. un
`button` con `text = "-"`) reciba un tamaño natural basado en su
contenido (medir el texto + padding por defecto), en vez de `0` o
"estirarse a llenar todo".

### Archivos a modificar

- `runtime/avaui/src/layout/LayoutEngineImpl.cpp`
  - Función `PlaceComponent` (línea ~72 en adelante): hoy, si no hay
    `width`/`height` explícito y el align no es `Stretch`, asigna `width
    = 0.0` / `height = 0.0` (comentado como limitación conocida de Fase
    3). Hay que reemplazar ese `0.0` por una llamada a una nueva función
    de medición (`MeasureIntrinsicSize` o similar) que dependa del
    `TypeName()` del componente (`Button`, `Text`, etc.) y de sus
    propiedades (`text`, `fontSize`, `fontName`, padding interno por
    defecto del control).
  - Necesita agregarse un **measure pass** antes (o integrado) del
    recorrido actual: hoy `LayoutNodeRecursive` solo hace un pase
    top-down (el padre reparte espacio a los hijos). Para intrinsic
    sizing hace falta que los hijos que no dependen del espacio del
    padre (texto, botones con texto fijo) puedan reportar su tamaño
    natural *antes* de que el padre reparta — un pase bottom-up simple
    (no hace falta el flexbox completo: alcanza con que cada tipo de
    nodo sepa auto-reportar su tamaño si no tiene `width`/`height`).

- `runtime/avaui/src/layout/LayoutEngineImpl.h`
  - Declarar la nueva función de medición (firma propuesta:
    `LayoutSize MeasureIntrinsic(IComponent* component) const;` o
    similar, según el nombre de tipos ya usados en el header).

- `runtime/avaui/src/layout/LayoutProperties.h` / `.cpp`
  - Hoy documenta (comentario visible en `LayoutProperties.h`): "width,
    height — Absent means auto: fill the space the parent offers ... —
    Phase 3 has no intrinsic/content-based sizing yet". Actualizar ese
    comentario una vez implementado el measure pass, y agregar ahí (o en
    un archivo nuevo, ver abajo) las constantes de padding/tamaño por
    defecto de cada tipo de control (ej. `kDefaultButtonPaddingX`,
    `kDefaultButtonPaddingY`, `kDefaultButtonMinHeight`).

- **Archivo nuevo:** `runtime/avaui/src/layout/TextMeasure.h` +
  `runtime/avaui/src/layout/TextMeasure.cpp` (o el nombre que siga la
  convención del proyecto)
  - Necesita una función de medición de texto independiente de
    plataforma: dado `text`, `fontSize`, `fontName`, devolver un ancho
    aproximado. Como hoy hay dos renderers (`GdiRenderer` para desktop,
    `HTMLRenderer` para web) que en teoría deben mostrar exactamente lo
    mismo, esta medición **no puede depender de GDI** (rompería la
    paridad con el render web) — debe ser una heurística propia
    (ej. ancho aproximado por carácter según tabla de anchos por fuente,
    o una tabla simple de "ancho promedio por char" si no se quiere
    empezar con medición exacta por glifo). Documentar explícitamente
    que es una aproximación y que ambos renderers (`GdiRenderer` y
    `HTMLRenderer`) consumen el mismo resultado — si no coincide con lo
    que el navegador realmente pinta, ese es un problema aparte a
    revisar después, no bloqueante para esta fase.

- `runtime/avaui/src/layout/LayoutNode.h` / `.cpp`
  - Revisar si `LayoutNode` necesita un campo para cachear el tamaño
    intrínseco ya calculado (evitar remedir en cada frame si el
    contenido no cambió).

### Criterio de aceptación

- Un `button` con `text = "-"` sin `width`/`height` en `runtime/avahost`
  renderizado con `render-static` o `render-dynamic` debe salir con un
  tamaño razonable (unas pocas decenas de píxeles de alto, ancho acorde
  al texto + padding), no 405×332px ni 0×0px.
- El mismo `.avaui` debe verse igual en `GdiRenderer` (desktop) y
  `HTMLRenderer` (web) — correr `avahost render-static` y comparar contra
  una captura de AvaStudio (que usa `GdiRenderer`).

---

## B. Dejar de cargar Tailwind/CSS por defecto

**Objetivo:** que ni el manifest por defecto ni el scaffold de `avahost
new` carguen Tailwind ni generen `.avaui` con `class=`.

### Archivos a modificar

- `runtime/avahost/src/config/app_manifest.cpp`
  - Función `DefaultAppManifest()` (línea ~105-118): hoy siempre agrega
    `{"css/app.css"}` y `{"https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"}`.
    Cambiar para que devuelva un `AppManifest` **vacío** (`manifest.resources`
    sin entradas) — un proyecto sin `app.ava` ya no debe traer Tailwind
    por defecto. Si en el futuro se quiere permitir CSS custom como
    opt-in explícito, debe quedar documentado que el usuario lo declara
    a mano en su propio `app.ava` con `import "css/lo-que-sea.css"` — el
    mecanismo de `ResourceImport`/`BuildHeadTags` ya soporta eso, solo
    hay que dejar de asumirlo por defecto.
  - Actualizar el comentario de la línea ~113 ("The Tailwind entry below
    is tooling...") que ya no aplica.

- `runtime/avahost/src/config/app_manifest.h`
  - Actualizar el comentario de cabecera (líneas 2-20) que documenta
    `import "css/app.css"` / `import "https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"`
    como ejemplo del manifest — ya no son el ejemplo por defecto.
    Actualizar también el comentario de `DefaultAppManifest()` (líneas
    56-60).

- `runtime/avahost/src/cli_commands.cpp` — función `CmdNew` (línea ~102
  en adelante), reescribir el scaffold generado por `avahost new`:
  - **`app.ava`** (línea ~128-133): quitar las dos líneas `import
    "https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"` e `import
    "css/app.css"`. Dejar el archivo vacío o solo con el comentario
    explicando que aquí se declaran recursos globales opcionales (y que
    por defecto no hay ninguno).
  - **`routes/index.avaui`** (línea ~135-162): quitar `class = "flex
    flex-col gap-4 p-8"`, `class = "text-2xl font-bold"`, `class =
    "text-gray-600"` y reemplazar por las primitivas nativas
    equivalentes usando las propiedades ya soportadas por el
    `LayoutEngine` (`padding`, `gap`, `align`, y — una vez resuelto el
    punto A — tamaños de fuente vía propiedades propias del `text`, no
    vía `class`).
  - **`layouts/main.avaui`** (línea ~171-195): mismo cambio — quitar
    `class = "min-h-screen flex flex-col"`, `class = "flex items-center
    justify-between p-4 border-b border-gray-200"`, `class =
    "font-semibold text-lg"`, `class = "text-blue-600 hover:underline"`,
    `class = "p-4 border-t border-gray-200 text-sm text-gray-500"` por
    las primitivas nativas (`row`/`column` con `padding`, `gap`, `align`,
    y bordes vía las propiedades nativas de borde que ya soporta
    `OnDrawRectangle`/`OnDrawEllipse` sin necesitar `class`).
  - **`wwwroot/css/app.css`** (línea ~197-207): el comentario del
    template dice explícitamente "AvaLang styling uses Tailwind utility
    classes directly via the `class` property" — reescribir ese
    comentario para reflejar la decisión nueva (sin Tailwind, usar
    primitivas nativas), o directamente dejar de generar este archivo
    si ya no hay ningún CSS que cargar por defecto.

### Criterio de aceptación

- `avahost new` en un proyecto nuevo, seguido de `avahost run` +
  `curl http://localhost:8080`, no debe traer ningún `<script
  src=".../tailwindcss/...">` ni `<link rel="stylesheet"
  href="/css/app.css">` a menos que el usuario lo haya declarado
  explícitamente en su propio `app.ava`.
- El HTML resultante debe estar compuesto solo por `div.ava-element`
  (o el fragmento que corresponda), sin ningún `div class="flex ..."`
  vacío de los que aparecían en el `curl` de esta sesión.

---

## C. El viewport no cubre toda la página

**Objetivo:** que el recuadro gris (`.ava-viewport`, hoy fijo a 1280×720
vía `runtime/avahost/src/rendering/ui_pipeline_static_renderer.h` /
`ui_pipeline_dynamic_renderer.cpp`, `viewportWidth`/`viewportHeight`) no
deje un borde/hueco blanco alrededor cuando la ventana del navegador es
más grande.

### Archivo a modificar

- `runtime/avaui/src/renderer/HTMLRenderer.cpp` — función
  `EmitHTMLHeader()` (línea ~65-83): hoy el `<style>` inline solo define
  `body { margin: 0; padding: 0; ... }` y `.ava-viewport { width: ...;
  height: ...; position: relative; overflow: hidden; }`, sin nada que
  centre el viewport dentro del `<body>` ni que iguale el fondo de la
  página al fondo del viewport. Dos cosas a decidir y aplicar aquí (son
  alternativas, elegir una):
  1. **Centrar el viewport de tamaño fijo** dentro de la ventana: agregar
     a `body` algo como `display: flex; justify-content: center;
     align-items: center; min-height: 100vh; background: <color de fondo
     del viewport, no blanco>;` para que no se vea como un recuadro
     flotando sobre blanco.
  2. **Hacer el viewport responsivo** (llenar toda la ventana en vez de
     un tamaño fijo 1280×720): esto es un cambio más grande porque
     `viewportWidth`/`viewportHeight` hoy son fijos en
     `RenderOptions` (`runtime/avahost/src/rendering/ui_pipeline_static_renderer.h`,
     líneas 40-41, y el análogo en `ui_pipeline_dynamic_renderer.cpp`) y
     el `LayoutEngine` calcula todo una sola vez contra ese tamaño fijo
     — pasar a "llenar la ventana" implicaría recalcular layout en
     cada resize del navegador (vía JS + un nuevo endpoint, o
     simplemente aceptar que el tamaño de referencia para desktop/mobile
     puede diferir). **Recomendado para esta fase: opción 1** (centrar,
     tamaño fijo) — es el cambio mínimo y no toca el pipeline de layout;
     dejar la opción 2 para una fase aparte si se decide que el viewport
     debe ser responsivo.

### Criterio de aceptación

- Con la ventana del navegador más grande que 1280×720, el recuadro gris
  debe verse centrado (o con el fondo de página igualado al del
  viewport), no como una caja perdida en una esquina sobre fondo blanco.

---

## D. Qué hacer con el soporte de `class=` que ya existe

Los tres draw functions de `runtime/avaui/src/renderer/HTMLRenderer.cpp`
(`OnDrawRectangle`, `OnDrawEllipse`, `OnDrawText`) y el de
`runtime/avaui/src/renderer/GdiRenderer.cpp` ya tienen una rama para
`className` (aunque `GdiRenderer` la ignora, ver punto 0). Dado que la
decisión es no usar `class=` en ningún `.avaui` del proyecto:

- **No es necesario borrar el soporte de `class=` de los renderers** —
  no hace daño que exista mientras nada lo use, y sacarlo sería un
  cambio de API sin beneficio inmediato.
- Sí conviene, una vez cerrados A/B/C, **dejar un comentario explícito**
  en `HTMLRenderer.cpp` (junto a las ramas `hasClass`) y en
  `GdiRenderer.cpp` documentando que `class=` es un escape hatch no
  recomendado (rompe paridad desktop/web, ver punto 0), y que el camino
  soportado es siempre row/column/padding/gap/width/height/align.
- Esta parte es solo documentación — no bloquea nada de A/B/C, se puede
  dejar para el final.

---

## 4. Resumen ejecutable (para empezar directo en otra sesión)

| Paso | Archivos principales | Bloquea a |
|---|---|---|
| A. Intrinsic sizing | `LayoutEngineImpl.cpp/.h`, `LayoutProperties.h/.cpp`, `TextMeasure.h/.cpp` (nuevo), `LayoutNode.h/.cpp` | B (parcialmente) |
| B. Quitar Tailwind/CSS por defecto | `app_manifest.cpp/.h`, `cli_commands.cpp` (scaffold `app.ava`, `routes/index.avaui`, `layouts/main.avaui`, `wwwroot/css/app.css`) | — |
| C. Viewport no cubre la página | `HTMLRenderer.cpp` (`EmitHTMLHeader`) | — |
| D. Documentar `class=` como no recomendado | `HTMLRenderer.cpp`, `GdiRenderer.cpp` (comentarios) | — |

Empezar por **A**, seguir con **B** y **C** en paralelo, cerrar con **D**.

---

## 5. Estado final

Las cuatro fases (A, B, C, D) están implementadas. Pendiente en todas
ellas, por falta de acceso a un toolchain Windows real en las
sesiones que las hicieron: compilar el proyecto completo (CMake +
vcpkg, MSVC), correr `avahost new` / `avahost run` / `render-static` /
`render-dynamic`, y confirmar visualmente contra `AvaStudio`
(`GdiRenderer`) y un navegador. Cada fase solo pudo verificarse con
`g++ -fsyntax-only -Wall -Wextra` sobre los archivos modificados de
forma aislada (sin dependencias de plataforma), sin errores en
ninguno de los casos donde eso fue posible.
