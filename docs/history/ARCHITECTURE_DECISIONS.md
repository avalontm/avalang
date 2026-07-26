# Architecture Decision Records (ADR) — índice

Registro breve de decisiones de arquitectura ya tomadas, con el
resultado y dónde está el detalle. No repite el razonamiento completo
— eso vive en la bitácora de sesiones correspondiente o en el
documento de arquitectura vigente. Se agrega una entrada nueva cada
vez que se cierra una decisión de fondo, no por cada cambio menor.

## ADR-001 — Extensión de archivo: `.avaui` (no `.avax`)

**Decisión**: los archivos de UI de AvaLang usan la extensión `.avaui`.
**Razón**: consistencia con el nombre "AvaUI" ya usado en todo el
proyecto.
**Detalle histórico**: `docs/history/DESIGNER_VIEW_SESSIONS.md`,
sección 0 (movida desde `08_DESIGNER_VIEW_PLAN.md`).

## ADR-002 — Formato de `.avaui`: texto AvaLang (`state`/`view`/`methods`), no JSON

**Decisión**: `.avaui` es texto en sintaxis AvaLang con bloques
`state`/`view`/`methods` (y `properties`/`import` reservados), no JSON.
**Razón**: un prototipo .NET (`AvaLang.UI`) ya implementaba y corría
esta sintaxis contra la misma C API (`ava_ui_*`); usar el mismo formato
evita un JSON paralelo y hace que un `.avaui` de Ava Studio y un
`.ava` de ese framework sean intercambiables.
**Estado actual**: `docs/architecture/17_AVAUI_FILE_FORMAT.md`.
**Detalle histórico**: `docs/history/DESIGNER_VIEW_SESSIONS.md`,
sección 0.1.

## ADR-003 — Parser `.avaui` canónico converge en `core`

**Decisión**: `core/src/ui/avaui_text.*` es el parser/writer canónico,
pensado para reemplazar tanto al parser propio de Studio
(`studio/src/design/avaui_text.*`) como a `avaui_json.*` (obsoleto tras
ADR-002).
**Estado actual**: `docs/architecture/10_AVAUI.md`, sección "Qué existe
hoy vs qué es roadmap".

## ADR-004 — El layout engine se promueve a `core/src/ui/`, operando sobre `Component`

**Decisión**: el algoritmo de layout (`ComputeLayout`) vive en
`core/src/ui/layout.*` y opera sobre `ava::ui::Component`, no sobre
`DesignNode`. La medición de leaves se inyecta vía un `LeafMeasurer`
en vez de una constante fija.
**Razón**: el algoritmo ya era agnóstico de ImGui; atarlo a
`DesignNode` bloquea a cualquier backend no-Studio. Ver
`docs/architecture/10_AVAUI.md` sección "Decisión: dónde vive el
layout engine" para las alternativas consideradas y sus trade-offs.
**Estado actual**: `docs/architecture/12_LAYOUT.md`.

## Cómo se relacionan estos ADRs con las bitácoras de sesión

Los ADRs de arriba son el resumen; las bitácoras completas (con el
razonamiento sesión por sesión, código intermedio y callejones sin
salida) están en:
- `docs/history/DESIGNER_VIEW_SESSIONS.md`
- `docs/history/DESIGNER_CANVAS_UX_SESSIONS.md`
