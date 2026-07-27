# Formato de archivo `.avaui`

## Purpose

Especifica el formato del archivo `.avaui`: cómo se declara una
pantalla de AvaUI en disco, qué bloques tiene y qué representa cada
uno. Es el contrato que leen/escriben el parser de `core/src/ui/` y el
Designer de Ava Studio.

## Responsibilities

- Definir la extensión (`.avaui`) y por qué no es JSON.
- Definir la sintaxis de los bloques `state`, `view`, `methods`,
  `properties` e `import`.
- Servir de referencia para el parser/writer canónico
  (`core/src/ui/avaui_text.*`, ver `10_AVAUI.md`).

## Current Implementation

### Extensión

Los archivos de UI de AvaLang usan la extensión **`.avaui`** (no
`.avax`) -- consistente con el nombre "AvaUI" usado en todo el proyecto
(`AVAUI_FRAMEWORK.md`, `bindings/csharp/AvaLang.UI`).

### El formato es texto AvaLang, no JSON

`.avaui` es texto plano en sintaxis AvaLang con bloques
`state`/`view`/`methods` (y `properties`/`import`, reservados), no
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

Van antes que `import`/`properties`/`state` en la convención real (ver
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

        button
            text = "Guardar"
            click = btnGuardar_Click
        end
    end
end

methods
    func btnGuardar_Click()
        -- handler
    end
end
```

### Bloques

- **`view`**: el árbol de componentes tal cual -- esto es lo que dibuja
  el lienzo (`ComputeLayout` + `designer_canvas.cpp`, ver `12_LAYOUT.md`
  y `16_STUDIO.md`). Mapea directo a `DesignNode`/`Component`: `type`,
  `id`, `properties`, `events`, `children`.
- **`state`**: variables iniciales del documento.
- **`methods`**: el code-behind real, en sintaxis AvaLang normal
  (`func nombre(params) ... end`). Es lo que se ve al presionar F7 en
  Ava Studio (ver `16_STUDIO.md` sección 2) -- mismo rol que un `.frm`
  de VB6 con su sección `Private Sub ... End Sub`, pero acá es
  simplemente el `TextEditor` mostrando el archivo `.avaui`.
- **`properties`**: propiedades del documento/componente en sí
  (ej. `title`).
- **`extends`/`route`**: ver sección dedicada arriba. No producen
  nodos en el árbol de `view` -- salen como campos separados
  (`ParsedAvaui::extends`/`::routes`), mismo tratamiento que
  `state`/`import`/`methods`.
- **Props de evento** (`click`, `onchange`, `oninput`, ...): se
  guardan aparte de las props de estilo, apuntando a un nombre de
  función que debe existir en `methods`. Es literalmente
  `DesignNode::events`.
- **Llamada a un componente importado**: `Navbar()` -- PascalCase, sin
  bloque `... end` propio en el sitio de la llamada. El árbol real de
  `Navbar` vive en `components/navbar.avaui` y se resuelve al cargar
  (`component_resolver.cpp`, ver `16_STUDIO.md`).

## Public Interfaces

- `core/src/ui/avaui_text.{h,cpp}` -- parser/writer canónico (ver
  `10_AVAUI.md` sección "Qué existe hoy vs qué es roadmap" para su
  estado de convergencia con el parser propio de Studio).

## Dependencies

- Consumido por `core/src/ui/` (parser canónico) y
  `studio/src/design/` (Designer, vía su propio parser todavía no
  convergido -- ver `10_AVAUI.md`).
- El árbol resultante (`view`) es la entrada de `12_LAYOUT.md`.

## Future Evolution

- Resolución completa de `state`/`methods` de un componente importado
  (`Navbar()`) -- hoy `component_resolver.cpp` resuelve la estructura
  pero no todo lo demás.

## Open Questions

Ninguna abierta específica de este documento -- ver `10_AVAUI.md`
sección 8 para las preguntas abiertas de la arquitectura de AvaUI en
general.
