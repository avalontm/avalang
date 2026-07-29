# Formato de archivo `.avaui`

## Purpose

Especifica el formato del archivo `.avaui`: cómo se declara una
pantalla de AvaUI en disco, qué bloques tiene y qué representa cada
uno. Es el contrato que leen/escriben el parser de `core/src/ui/` y el
Designer de Ava Studio.

## Responsibilities

- Definir la extensión (`.avaui`) y por qué no es JSON.
- Definir la sintaxis de los bloques canónicos: `properties`, `state`, 
  `view`, `code`, `style` e `import`.
- Documentar bloques legacy (`metadata`, `methods`) soportados para
  lectura pero nunca emitidos en escritura.
- Servir de referencia para el parser/writer canónico
  (`core/src/ui/avaui_text.*`, ver `10_AVAUI.md`).

## Current Implementation

### Extensión

Los archivos de UI de AvaLang usan la extensión **`.avaui`** (no
`.avax`) -- consistente con el nombre "AvaUI" usado en todo el proyecto
(`AVAUI_FRAMEWORK.md`, `bindings/csharp/AvaLang.UI`).

### El formato es texto AvaLang, no JSON

`.avaui` es texto plano en sintaxis AvaLang con bloques
`state`/`view`/`methods` (y `metadata`/`import`, reservados), no
JSON. Esta decisión reemplazó una propuesta original en JSON -- ver
`docs/history/ARCHITECTURE_DECISIONS.md` (ADR-002) y
`docs/history/DESIGNER_VIEW_SESSIONS.md` para el detalle histórico de
por qué se abandonó JSON: un prototipo .NET (`AvaLang.UI`) ya
implementaba y corría esta sintaxis contra la misma C API (`ava_ui_*`)
que usa este repo, así que usar el mismo formato evita un JSON
paralelo y hace que un `.avaui` de Ava Studio y un `.ava` de ese
framework sean intercambiables.

### `extends` / `route` (páginas)

Dos líneas de nivel superior (columna 0), reservadas, opcionales,
independientes de `import` -- pensadas para páginas (no para
componentes importados ni layouts):

- **`extends "layout"`**: la página hereda el layout `layout.avaui`
  (resuelto por nombre, ver `PLAN_LAYOUTS.md` del lado .NET). A lo
  sumo una es significativa por archivo -- si hay más de una, gana la
  primera.
- **`route "/path/{param}"`**: declara una ruta que sirve esta página
  (file-based routing). Puede repetirse -- una misma página puede
  responder a varias rutas (ver `productos.avaui` en `avalang-dotnet`
  para un ejemplo real con 3). Cada segmento `{nombre}` de la
  plantilla es un parámetro; `{nombre?}` lo marca opcional y
  `{nombre:constraint}` le agrega una restricción (`int`, `long`,
  `guid`, `slug`, `alpha`, ...) -- la restricción se guarda como texto
  tal cual, no se valida en este parser.

Van antes que `import`/`metadata`/`state` en la convención real (ver
`dashboard.avaui`, `admin.avaui` en `avalang-dotnet`):

```
extends "admin"

route "/admin/dashboard"

state
    visitas = 128
end
...
```

### Ejemplo

```
import "components/navbar"

properties
    title = "Mi App"
end

state
    counter = 0
end

view
    column
        fill = "true"
        padding = 20
        gap = 16

        Navbar()

        text
            value = "Counter: " + counter
            fontSize = 32
        end

        button Guardar
            text = "Guardar"
        end
    end
end

code
    func OnGuardarClick()
        -- se enlaza automáticamente por convención
    end
end
```

### Bloques Canónicos

- **`properties`**: propiedades del documento/componente en sí (ej. `title`). 
  Para páginas, AvaHost (`avahost/src/rendering/html_renderer.cpp`, 
  `BuildPageMeta`) reconoce: `title` (requerida), `description`, `image`, 
  `url`, `siteName`, `ogType` (default `"website"`), `twitterCard` (default 
  `"summary_large_image"` si hay `image`, si no `"summary"`) -- generan 
  `<title>` más los `<meta>` de Open Graph/Twitter Card para compartir en 
  redes. En una página con `extends`, este bloque vive en la página, no en 
  el layout. **Es lo que se emite siempre en `WriteAvauiText()`**.
- **`state`**: variables iniciales del documento/componente. Se guardan como
  pares `[key]=[value]` en `ParsedAvaui::state`.
- **`view`**: el árbol de componentes -- esto es lo que dibuja el lienzo 
  (`ComputeLayout` + `designer_canvas.cpp`, ver `12_LAYOUT.md` y 
  `16_STUDIO.md`). Mapea directo a `Component`: `type`, `id`, `properties`, 
  `events`, `children`. Soporta sintaxis abreviada: `button Guardar` 
  equivale a `button end { id = "Guardar" }`.
- **`code`**: el code-behind real, en sintaxis AvaLang normal (`func nombre(params) ... end`). 
  Contiene:
  - Funciones de ciclo de vida (`OnLoad`, `OnShow`, `OnHide`, `OnUnload`)
  - Manejadores de eventos (automáticamente enlazados por convención: `OnIdEventName`)
  - Métodos auxiliares y lógica de negocio
  
  Es lo que se ve al presionar F7 en Ava Studio (ver `16_STUDIO.md` sección 2) -- 
  mismo rol que un `.frm` de VB6 con su sección `Private Sub ... End Sub`.
  
- **`style`**: definición de apariencia visual del componente. Se guarda como 
  pares `[key]=[value]` en `ParsedAvaui::style`, mismo formato que `state`.
  Puede contener: colores, tamaños, bordes, fuentes, temas, animaciones visuales.

- **`extends`/`route`**: ver sección dedicada arriba. No producen nodos en 
  el árbol de `view` -- salen como campos separados (`ParsedAvaui::extends`/`::routes`).

### Bloques Legacy (soportados en lectura, nunca en escritura)

- **`metadata`**: alias antiguo de `properties`. Se lee pero `WriteAvauiText()` 
  nunca lo emite -- siempre emite `properties` en su lugar.
- **`methods`**: alias antiguo de `code`. Se lee pero `WriteAvauiText()` 
  nunca lo emite -- siempre emite `code` en su lugar.

### Automatic Event Binding (Auto-bind)

Si un componente tiene un `id` (incluyendo sintaxis abreviada `button Guardar`) 
y existe una función en `code` que sigue la convención `On{PascalId}{PascalEvent}`, 
se enlaza automáticamente **sin necesidad de escribir `click = ...` explícitamente**:

```
button Guardar        -- id implícito: "Guardar"
end

code
    func OnGuardarClick()   -- se enlaza automáticamente
        -- handler aquí
    end
end
```

Un `click = OnGuardarClick` explícito en `view` siempre gana (toma precedencia) 
sobre el auto-bind. Esto simplifica la sintaxis común manteniendo control fino cuando 
se necesita.

### Llamadas a Componentes Importados

`Navbar()` -- PascalCase, sin bloque `... end` propio en el sitio de la llamada. 
El árbol real de `Navbar` vive en `components/navbar.avaui` y se resuelve al 
cargar (`component_resolver.cpp`, ver `16_STUDIO.md`).

## Public Interfaces

- `core/src/ui/avaui_text.{h,cpp}` -- parser/writer canónico:
  - `ParseAvauiText(const std::string& text)`: parseea `.avaui` a `ParsedAvaui` 
    (estructura con `root`, `state`, `style`, `methods_text`, `imports`, `extends`, `routes`).
  - `WriteAvauiText(...)`: emite `ParsedAvaui` de vuelta a texto, siempre usando 
    bloques canónicos (`properties`, `state`, `view`, `code`, `style`).
  - `IsEventPropertyName(...)`: detecta nombres de evento válidos.
  - Helpers: `StateToJson`, `ImportsToJson`, `RoutesToJson`, etc.

## Dependencies

- Consumido por `core/src/ui/` (parser canónico) y
  `studio/src/design/` (Designer, vía su propio parser todavía no
  convergido -- ver `10_AVAUI.md`).
- El árbol resultante (`view`) es la entrada de `12_LAYOUT.md`.

## Future Evolution

- Cruzar el bloque `style` a través del C ABI (`avalang.h`/`c_api.cpp`) para 
  que AvaHost y Studio puedan leer/escribirlo completamente.
- Ejecutar funciones de ciclo de vida (`OnLoad`, `OnShow`, `OnHide`, `OnUnload`) 
  en AvaHost a través del request handler.
- Resolución completa de `state`/`code` de un componente importado 
  (`Navbar()`) -- hoy `component_resolver.cpp` resuelve la estructura visual 
  pero no ejecuta su lógica ni maneja bindings de eventos.

## Open Questions

- ¿Cómo manejar herencia de `style` entre componentes importados (layouts, 
  temas globales)?
- ¿Soportar propiedades polimórficas (ej., `properties` que acepten 
  sobrescrituras de subcomponentes)?
- Ver `10_AVAUI.md` sección 8 para preguntas abiertas de arquitectura AvaUI 
  en general.
