# Ava Ecosystem — Arquitectura del Sistema

## Purpose

Punto de entrada para cualquiera que quiera entender el proyecto Ava
antes de profundizar en el detalle de cada pieza. Un único diagrama
maestro + un índice de qué documento cubre qué + una tabla de estado
real (implementado vs roadmap). No repite el detalle de cada capa --
para eso están los documentos enlazados.

## Diagrama maestro

```
                    Ava Ecosystem

                +------------------+
                |    Ava Studio    |
                +------------------+
                          |
                          v
                +------------------+
                |      AvaUI       |
                +------------------+
                          |
        +-----------------+-----------------+
        v                                   v
  Component Tree                     Layout Engine
        |                                   |
        +-----------------+-----------------+
                          |
                          v
                +------------------+
                | AvaLang Runtime  |
                +------------------+
                          |
              Compiler -> Bytecode -> VM
                          |
                          v
                     Stable C API
                          |
        +-----------------+-----------------+
        v                                   v
       C# binding                      (otros hosts,
   (bindings/csharp)                     roadmap)
```

Nota: este diagrama muestra las piezas principales y su dirección de
dependencia, no cada módulo. "Render pipeline" y "Event System" no
tienen todavía documento propio (ver la tabla de huecos más abajo);
hoy ambos viven implementados dentro de Ava Studio (`designer_canvas.cpp`),
no como capas separadas.

## Qué existe hoy vs qué es roadmap

| Pieza | Estado | Doc de referencia |
|---|---|---|
| VM + compiler + C API | Implementado, estable | (sin doc propio en esta serie todavía) |
| `Component` tree runtime + C API | Implementado | `11_COMPONENT_TREE.md` |
| Formato de archivo `.avaui` (texto AvaLang) | Implementado | `17_AVAUI_FILE_FORMAT.md` |
| Parser/writer `.avaui` canónico (`core/src/ui/avaui_text.*`) | Implementado | `10_AVAUI.md` |
| `DesignNode` (mirror de editor en Studio) | Implementado, pendiente de converger con `Component` | `10_AVAUI.md` |
| Resolución de `import`/`Componente()` | Implementado (solo entre `DesignNode`) | `16_STUDIO.md` |
| Layout engine (Column/Row/Stack) | Implementado, hoy solo en Studio sobre `DesignNode`, con plan de promoción a `core` | `12_LAYOUT.md` |
| Layout Grid/Flex | Roadmap -- caen en fallback de columna | `12_LAYOUT.md` |
| Render pipeline (ImGui, vía Ava Studio) | Implementado, un solo backend | `16_STUDIO.md` |
| Widget lifecycle formal (mount/update/unmount) | No existe como concepto propio, lógica dispersa | -- |
| UI builtins en la VM (`page()`, `stack()`, `button()`, ...) | Roadmap, 0% | `AVAUI_FRAMEWORK.md` |
| API de enumeración de propiedades (`ava_ui_list_properties`) | Roadmap | `11_COMPONENT_TREE.md` |
| Backend de render HTML5/CSS | Roadmap, 0% | `AVAUI_FRAMEWORK.md` |
| Backend de render nativo (controles del SO) | Roadmap, 0% | `AVAUI_FRAMEWORK.md` |
| Runtime headless (correr un `.avaui` sin Studio) | Roadmap, 0% | `AVAUI_FRAMEWORK.md` |
| Binding C# (`bindings/csharp`) migrado a la C API actual | Pendiente, fuera de alcance de este repo por ahora | -- |

## Índice de documentación

### Visión
- `AVAUI_FRAMEWORK.md` -- hacia dónde apunta AvaUI. No usar
  para saber qué existe hoy.

### Arquitectura (estado actual)
- `10_AVAUI.md` -- el problema de fondo: dos implementaciones del
  árbol de UI (`core::Component` vs `DesignNode`) y el plan de
  convergencia.
- `11_COMPONENT_TREE.md` -- `Component`/`ComponentTree`/`LayoutType` y
  su C API.
- `12_LAYOUT.md` -- el algoritmo de layout, as-built y su firma nueva
  post-migración a `core`.
- `16_STUDIO.md` -- Ava Studio como aplicación: paneles, tabs, los
  tres árboles de UI que conviven en memoria, y su relación con
  `core/src/ui`. Dependencias, estructura de carpetas, syntax
  highlighting y comandos de build siguen únicamente en
  `AGENTS_STUDIO.md` por ahora.
- `17_AVAUI_FILE_FORMAT.md` -- el formato de texto `.avaui`.

### Huecos conocidos (referenciados desde otros docs, todavía sin escribir)
- **Render pipeline** -- documentar `designer_canvas.cpp` como
  implementación de referencia del backend #1, separando qué es
  genérico de qué es ImGui-específico.
- **Widget lifecycle** -- formalizar mount/update/unmount y evaluación
  de props, hoy disperso entre `designer_canvas.cpp` y `state_eval.cpp`.
- **Render backends (contrato)** -- qué debe cumplir un backend nuevo.
- **VM / compiler / bytecode / C API** -- no hay doc de arquitectura
  dedicado a esto todavía; lo que existe está disperso en referencias
  desde los docs de AvaUI.
- **Bindings** (`bindings/csharp`) -- sin doc de arquitectura propio.

### Historia (bitácoras de sesión, no arquitectura vigente)
- `docs/history/ARCHITECTURE_DECISIONS.md` -- índice de decisiones
  cerradas (ADRs), con su razón y dónde quedó implementada.
- `docs/history/DESIGNER_VIEW_SESSIONS.md` -- construcción de la vista
  Design del Designer, sesión por sesión.
- `docs/history/DESIGNER_CANVAS_UX_SESSIONS.md` -- mejoras de
  interacción/estética del canvas, sesión por sesión.

### Pendientes de eliminar
- `AGENTS_STUDIO.md` y `AVAUI_FRAMEWORK.md` quedan sin tocar a
  propósito -- se van a eliminar directamente, no migrar. Si en algún
  momento hace falta algo de ahí (dependencias/build/syntax
  highlighting de `AGENTS_STUDIO.md`, o el catálogo de componentes de
  `AVAUI_FRAMEWORK.md`), rescatarlo antes de borrarlos.

## Convención de estilo de esta serie

Cada documento de arquitectura sigue esta estructura:

1. **Purpose** -- qué es y para qué se lee.
2. **Responsibilities** -- qué cubre (y, cuando ayuda, qué no cubre).
3. **Current Implementation** -- cómo funciona hoy.
4. **Public Interfaces** -- APIs/contratos que expone.
5. **Dependencies** -- de qué depende y quién depende de él.
6. **Future Evolution** -- qué es roadmap, sin fecha.
7. **Open Questions** -- decisiones todavía no tomadas.

Los documentos heredados de antes de esta convención (`10_AVAUI.md`,
`12_LAYOUT.md`, `16_STUDIO.md`) usan una numeración de secciones propia
en vez de este esqueleto exacto -- no se reescribieron sección por
sección para no perder precisión técnica, pero cubren el mismo
contenido. Los documentos nuevos (`11_COMPONENT_TREE.md`,
`17_AVAUI_FILE_FORMAT.md`) sí siguen la plantilla al pie de la letra.
