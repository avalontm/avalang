# AvaUI — Plan de sub-fases: Fase 19 (Animation)

Basado en el estado real del repo tras Fases 12-18 (✅ todas, ver
`docs/AVALANG_UI_PROGRESS.md`). Punto de partida ya existente que esta
fase reutiliza sin romper ABI congelada (F13):

- `ISceneNode` (Fase 7) ya expone `Transform`/`SetLocalTransform`,
  `Opacity`/`SetOpacity`, `MarkDirty()`/dirty region tracking.
- `IState`/`StateBinding` (Fase 4) ya notifica cambios de propiedad.
- `IEventDispatcher` (Fase 5) ya despacha click y otros eventos.
- Parser `.avaui` (Fase 14) y Controls (Fase 17-18) ya construyen árbol
  real end-to-end.

Alcance: solo Windows (mismo criterio que Fases 1-18). Linux/macOS:
stub directo, sin trabajo nuevo.

---

## 19.1 — Núcleo: tipos y easing

**No toca Scene Graph todavía.**

- `IAnimatable`/`AnimatableValue`: valor interpolable genérico (float,
  `Transform.position/scale/rotation`, `opacity`). Reusa los campos ya
  existentes en `ISceneNode` — no crea propiedades nuevas.
- Funciones de easing (`Linear`, `EaseInOut`, etc.), puras, sin estado.

**Entregable:** `ui/include/avalang/ui/animation/IAnimatable.h` +
`Easing.h`. Testeable aislado, sin dependencia de Scene Graph.

---

## 19.2 — AnimationClock + Timeline

- `IAnimationClock`: reloj de frame (delta time), fuente única de
  tiempo para todas las animaciones (evita que cada animación lea el
  tiempo por su cuenta).
- `Timeline`/`Keyframe`: lista ordenada de `(tiempo, valor, easing)`
  para una propiedad animable.

**Entregable:** `ui/src/animation/AnimationClock.*`, `Timeline.*`. Sin
integrar a Scene Graph todavía — testeable aislado.

---

## 19.3 — AnimationController + integración con Scene Graph

- `AnimationController`: dueño de N `Timeline`s activos. `Update(dt)`
  calcula el valor interpolado y lo escribe en
  `ISceneNode::SetLocalTransform`/`SetOpacity` (Fase 7, sin romper la
  ABI congelada en F13).
- Play / Pause / Stop / Loop / PingPong.
- Cada `Update` dispara `MarkDirty()` del `SceneNode` afectado (reusa
  el dirty tracking ya existente, no inventa uno nuevo).

**Entregable:** primera animación corriendo de verdad contra un
`SceneNode` real (no mock).

---

## 19.4 — Disparo declarativo desde `.avaui` y desde `IState`

- Extensión mínima al parser (Fase 14): bloque `animate` en `.avaui`
  → crea un `Timeline` al construir el árbol.
- Disparo por evento (`IEventDispatcher`, F5 — ej. click de Button →
  Play) y por cambio de estado (`StateBinding`, F4 — ej. `isChecked`
  → anima opacity).

**Entregable:** política de qué dispara qué, documentada (mismo estilo
que la política de errores del parser en F14).

---

## 19.5 — Test end-to-end

- Demo: Button con animación de opacity/scale al click, corrida real
  (`g++`, headless, igual que F14/F17/F18) → HTML con
  transform/opacity cambiando frame a frame.

**Entregable:** `ui/tests/AnimationDemo.cpp` corrido y verificado,
`docs/AVAUI_FASE19_ANIMATION.md`, `UIModule::AbiVersion()` bump.

---

## Orden de dependencia

```
19.1 -> 19.2 -> 19.3 -> 19.4 -> 19.5
```

Cada sub-fase bloquea la siguiente, mismo criterio que el resto del
plan (`AVAUI_PLAN_FASE12_PLUS.md`). No se avanza de sub-fase sin que
la anterior compile/corra (headless, `g++ -std=c++20`) tal como en
Fases 14/17/18.
