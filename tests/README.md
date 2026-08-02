# tests/

Tests del agente. Cubre correctness, performance, regresiones y compatibilidad de toda la plataforma.

## Qué va aquí

Tests automatizados que validan comportamiento del sistema. Siempre dentro de `tests/<subcarpeta>/`, **nunca** adentro de `runtime/<proyecto>/`.

## Subcarpetas y sus tipos

- `integration/` — cruzan varios proyectos (avahost + avaui + avalang en conjunto, end-to-end con fixtures reales).
- `regression/` — anti-regresión contra bugs ya arreglados (Gap A/B/C/D de Fase 20.2, etc.).
- `benchmarks/` — medir performance (latencia render, throughput requests, etc.).
- `stress/` — carga, concurrencia, recursos, memory leaks.
- `performance/` — análisis fino de performance (profiling, flamegraphs, hotspots).
- `compatibility/` — validar contra specs externas (HTML/CSS standards, browser engines).

## Qué NO va aquí

- Código de `runtime/<proyecto>/`.
- Demos de UI standalone.
- Fixtures internas de tests unitarios de un solo módulo (esos van en `runtime/<proyecto>/` según la convención del proyecto).
- Código de muestra (eso va en `samples/`).

## Regla clave para el agente

Si un test necesita tocar más de un proyecto de `runtime/`, va en `tests/integration/`. Si valida que un bug arreglado no vuelva, va en `tests/regression/`. Si mide cuánto tarda algo, va en `tests/benchmarks/` o `tests/performance/` según el nivel de análisis. **Nunca** se pone un test adentro de `runtime/`.

## Estructura actual

```
tests/
├── README.md                     ← este archivo
├── integration/
│   ├── README.md                 (placeholder)
│   ├── avahost_fixtures/         ← fixtures .avaui usadas por tests de avahost
│   └── avaui_demos/              ← demos end-to-end del motor avaui/ + fixtures
├── regression/    (vacío)
├── benchmarks/    (vacío)
├── stress/        (vacío)
├── performance/   (vacío)
└── compatibility/ (vacío)
```

Cuando agregues tests en una subcarpeta, documentar adentro qué cubre cada uno.
