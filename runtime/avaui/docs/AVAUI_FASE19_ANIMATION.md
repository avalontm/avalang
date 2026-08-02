# Fase 19 — Animation

**Objetivo:** sistema de animaciones para `avalang.ui.dll` — valor
interpolable genérico + easing, reloj de frame + timeline de
keyframes, `AnimationController` que escribe contra Scene Graph (Fase
7) sin romper la ABI congelada en F13, disparo declarativo desde
`.avaui` (`animate` block) y desde clicks/estado, y un demo
end-to-end real. Ver `AVAUI_FASE19_PLAN.md` para el plan de sub-fases
original.

**Status:** ✅ **CLOSED** (Windows/generic pipeline; ver "Alcance"
abajo)

---

## Alcance

Solo Windows, mismo criterio que Fases 1-18. Esta fase en particular no
toca ningún código específico de plataforma (`ui/src/platform/`,
`GdiRenderer`) — todo el trabajo vive en `ui/include/avalang/ui/animation/`
y `ui/src/animation/`, agnóstico de SO. Linux/macOS no requieren stub
nuevo porque no hay ninguna superficie de plataforma en esta fase.

---

## 19.1 — Núcleo: tipos y easing

`ui/include/avalang/ui/animation/IAnimatable.h`:
- `AnimatableKind` (`Float` | `Vec2`) y `AnimatableProperty`
  (`Opacity`/`Position`/`Scale`/`Rotation`) — `KindOf(property)` es el
  único punto que mapea una a la otra, para que `AnimationController`
  y `WireAnimations` no dupliquen el mismo switch.
- `AnimatableValue` (struct con `kind`, `f`, `v2`) + `Lerp(a, b, t)` —
  interpolación puramente lineal; el easing se aplica *antes* de
  llamar a `Lerp` (separación de responsabilidades explícita, ver
  comentario en `Timeline::SampleAt`).

`ui/include/avalang/ui/animation/Easing.h`:
- `EasingFunction` (`Linear`, `EaseIn`, `EaseOut`, `EaseInOut`,
  `EaseInCubic`, `EaseOutCubic`) + `ApplyEasing(fn, t)`.
- `EasingFromString(name)` — texto tal como se escribe en `.avaui`
  (`"ease-in-out"`, etc.) → enum, con fallback a `Linear` (gap suave,
  misma política que `CanonicalTypeName` de `AvauiParser`).

Todo header-only (funciones `inline`), sin `.cpp` — mismas razones que
documenta el propio header: puro, sin estado, testeable aislado sin
tocar Scene Graph. No se tocó `ui/CMakeLists.txt` en esta sub-fase (no
hay fuente `.cpp` que agregar).

---

## 19.2 — AnimationClock + Timeline

`ui/include/avalang/ui/animation/IAnimationClock.h` (interfaz pública,
factory `Create()`): `Tick(deltaSeconds)` — reloj de tick manual, no
wall-clock interno, misma filosofía que `IEventDispatcher::PollInput()`
(quien tiene el frame loop decide su propio `dt`). `DeltaTime()`,
`Elapsed()`, `Reset()`.

`ui/src/animation/AnimationClock.h/.cpp` (interno): implementación
concreta, `final`, no copiable — misma convención que
`Component`/`LayoutNode`/`SceneNode`.

`ui/src/animation/Timeline.h/.cpp` (interno): `Keyframe{time, value,
easing}` + `Timeline::AddKeyframe`/`Duration`/`SampleAt(t)`. El easing
de un keyframe se aplica al *segmento que termina en él* (no al que
empieza) — misma convención que CSS/la mayoría de herramientas de
animación. `SampleAt` sostiene el primer valor antes del primer
keyframe y el último después del último — nunca extrapola.

Ninguno de los dos integra Scene Graph todavía, tal como pide el plan
— eso es 19.3. `ui/CMakeLists.txt` actualizado con
`AnimationClock.cpp`/`Timeline.cpp`.

---

## 19.3 — AnimationController + integración con Scene Graph

`ui/include/avalang/ui/animation/AnimationController.h` (pública):
`AnimationHandle` (entero opaco), `PlaybackMode` (`Once`/`Loop`/
`PingPong`), factory `Create(ISceneGraph*)` (no-propietario, mismo
patrón que el resto del módulo). `Play(target, property, from, to,
duration, easing, mode)` — valida `duration > 0` y que
`from.kind`/`to.kind` coincidan con `KindOf(property)`; si no,
devuelve `kInvalidAnimationHandle` sin agendar nada (gap suave, no
excepción). `Pause`/`Resume`/`Stop`/`IsPlaying`/`Update(dt)`.

`ui/src/animation/AnimationControllerImpl.h` + `AnimationController.cpp`
(interno + factory, mismo patrón `StateBindingImpl`/`StateBinding.cpp`):
cada animación activa es un `Timeline` de 2 keyframes (`from`
en `t=0`, `to` en `t=duration`). `Update(dt)` avanza `elapsed` por
animación, calcula el cursor según `PlaybackMode` (`Once` clampa y se
detiene al llegar a `duration`; `Loop` usa `fmod`; `PingPong` rebota
`0 -> duration -> 0`), muestrea el `Timeline` y escribe el resultado
en el `ISceneNode` real vía `sceneGraph->FindNode(target)`:
`SetOpacity()` para `Opacity`, `SetLocalTransform()` (leyendo
`LocalTransform()` primero, para tocar solo el eje animado) para
`Position`/`Scale`/`Rotation`. Después de cualquiera de los dos,
llama a `MarkDirty()` explícitamente — **hallazgo real de esta
sub-fase**: `SceneNode::SetOpacity()` (Fase 7) *no* llama a
`MarkDirty()` por su cuenta (a diferencia de `SetLocalTransform()`,
que sí), así que sin esta llamada explícita una animación de opacidad
pura nunca marcaría dirty region. Documentado en el propio header en
vez de tocar `ISceneNode` (interfaz congelada desde F13) — se resuelve
enteramente del lado de `AnimationController`, sin reabrir Fase 7.

Un `target` que no resuelve a ningún `ISceneNode` es gap suave: el
cursor de esa animación sigue avanzando en `Update()`, simplemente no
hay nada donde escribir ese frame.

`ui/CMakeLists.txt` actualizado con `AnimationController.cpp`.

---

## 19.4 — Disparo declarativo desde `.avaui` y desde `IState`

**Extensión del parser** (`ui/include/avalang/ui/parser/AvauiParser.h` +
`ui/src/parser/AvauiParser.cpp`): un bloque `animate` anidado dentro de
cualquier componente en `view`, mismo estilo `key = value` que
`properties`/`state`/`style`:

```
button AnimatedButton
    text = "Fade Me"
    animate
        property = "opacity"
        from = "1.0"
        to = "0.2"
        duration = "0.4"
        easing = "ease-in-out"
        trigger = "click"
        mode = "once"
    end
end
```

Nuevo struct `parser::AnimationSpec` (todo texto crudo, como
`ParsedAvaui::state`/`code` ya hacían) + `ParsedAvaui::animations`
(vector, uno por bloque `animate` encontrado). **Decisión de
dependencia deliberada**: el parser no resuelve `property`/`from`/`to`/
`easing`/`mode` a los enums de `animation/` — solo guarda el texto tal
cual — para no hacer que `parser/` dependa de `animation/` (`Fwd.h`
documenta `Components -> Layout -> ... `, y `parser/` ya depende solo
de `components/`; `animation/` depende de `scene/`). Esa conversión
vive en `animation::WireAnimations`, que sí puede depender de ambos.

**`ui/include/avalang/ui/animation/AnimationBinding.h`** +
`ui/src/animation/AnimationBinding.cpp`: `WireAnimations(specs,
controller, dispatcher, states)`. Política de disparo documentada en
el propio header (y resumida acá, tal como pide 19.4 del plan):

- `trigger = "click"`: se suscribe un `IEventHandler` interno a
  `EventType::Click` sobre el `ComponentId` del propio componente
  (Fase 5). Cada click vuelve a llamar
  `AnimationController::Play()` con los mismos parámetros — reinicia
  desde `from` cada vez, no alterna/reanuda (mismo comportamiento
  "acción incondicional" que ya tiene `ButtonClickCallback`).
- `trigger = <nombre>` (cualquier otro texto): se busca en el mapa
  `name -> IState*` que el *caller* construye (`ParsedAvaui::state`
  solo tiene strings crudos — instanciar `IState` real por cada
  entrada de `state` es responsabilidad de quien posee el runtime del
  documento, mismo gap que F14/F18 ya documentan para `state`/`code`).
  Si se encuentra, se suscribe a los cambios: valor `PropertyType::Bool`
  dispara solo al volverse `true` (estilo `isChecked`); cualquier otro
  tipo dispara en cada cambio.
- `trigger` vacío o no encontrado en `states`: gap suave, no se
  suscribe nada — la animación queda jugable solo llamando
  `AnimationController::Play()` manualmente.

Los `IEventHandler` de un trigger por click se guardan en un registro
interno con vida de proceso (`g_clickHandlers`, mismo patrón que
`controls::Button::g_buttonCallbacks`) — no hay `Unwire()` en esta
fase.

`ui/CMakeLists.txt` actualizado con `AnimationBinding.cpp`.

---

## 19.5 — Test end-to-end

`ui/tests/fixtures/animation_demo.avaui`: un `Button` real con un
bloque `animate` (`opacity` 1.0 → 0.2, `duration = 0.4`,
`easing = "ease-in-out"`, `trigger = "click"`, `mode = "once"`).

`ui/tests/AnimationDemo.cpp` (mismo patrón que
`ButtonDemo.cpp`/`ControlsDemo.cpp` — `TestLogger`, asserts que lanzan
excepción, saldo 0/1): parsea el fixture, corre
Theme → Layout → RenderTree → SceneGraph, crea `AnimationController` +
`IEventDispatcher`, llama `WireAnimations`, verifica opacidad inicial
1.0, **despacha un evento Click sintético** (`events::MouseEvent`,
header privado `events/Event.h`, mismo patrón que
`commands/RenderCommandSink.h` en los demos anteriores) contra el
`ComponentId` real del botón parseado, y corre `Update(dt)` a través de
varios frames simulados (`dt = 0.1`, 5 llamadas para una duración de
`0.4`), verificando que la opacidad decrece monótonamente hasta
asentarse en `0.2` y ya no se mueve más allá de la duración
(`PlaybackMode::Once`). Renderiza HTML real en tres momentos
(`_0_before.html`, `_1_mid.html`, `_2_end.html`) para que el cambio de
`opacity` en el CSS generado (`HTMLRenderer`, Fase 10) sea visible en
el propio archivo, no solo en un assert de C++.

`UIModule::AbiVersion()` bump a **18** (`ui/src/UIModule.cpp` +
comentario en `UIModule.h`, fila nueva documentando toda la fase).
Target `ava_animation_demo` agregado a `ui/CMakeLists.txt` bajo
`AVA_BUILD_UI_TESTS` (OFF por defecto, mismo patrón que los demos de
F14/F17/F18).

---

## Limitaciones documentadas a propósito

- Sin reproducción inversa (`PingPong`/`Loop` sí soportados, pero no
  hay "reverse on state false" — un `trigger` de estado booleano solo
  reacciona a `true`, ver `AnimationBinding.h`).
- Sin `Unwire()` — ni para el registro de `IEventHandler` de clicks ni
  para las suscripciones a `IState`; mismo tipo de gap que
  `BindButtonClick`/`UnbindButtonClick` sí resuelve para Button pero
  esta fase no replica todavía.
- El parser no valida semánticamente el contenido de un `animate`
  block (`property`/`easing`/`mode` con texto no reconocido caen en
  fallbacks silenciosos dentro de `WireAnimations`, no en el parser) —
  gap suave documentado, consistente con la política de errores de
  Fase 14.
- Un componente con más de un bloque `animate` ya funciona (cada uno
  produce su propio `AnimationSpec`), pero no está probado
  explícitamente en el demo de 19.5 (que usa exactamente uno).
- No compilado con MSVC real (sin toolchain Windows en este sandbox,
  igual que todas las fases anteriores) — el código fue escrito y
  revisado contra las interfaces reales del repo (`ISceneNode`,
  `IState`, `IEventDispatcher`, `AvauiParser`) pero no ejecutado en
  esta sesión, a pedido explícito de la persona (compilar/probar queda
  para su propio equipo).

---

## Archivos nuevos/tocados

**Nuevos:**
- `ui/include/avalang/ui/animation/IAnimatable.h`
- `ui/include/avalang/ui/animation/Easing.h`
- `ui/include/avalang/ui/animation/IAnimationClock.h`
- `ui/include/avalang/ui/animation/AnimationController.h`
- `ui/include/avalang/ui/animation/AnimationBinding.h`
- `ui/src/animation/AnimationClock.h` / `.cpp`
- `ui/src/animation/Timeline.h` / `.cpp`
- `ui/src/animation/AnimationControllerImpl.h`
- `ui/src/animation/AnimationController.cpp`
- `ui/src/animation/AnimationBinding.cpp`
- `ui/tests/AnimationDemo.cpp`
- `ui/tests/fixtures/animation_demo.avaui`
- `docs/AVAUI_FASE19_ANIMATION.md` (este documento)

**Tocados:**
- `ui/include/avalang/ui/parser/AvauiParser.h` (`AnimationSpec`,
  `ParsedAvaui::animations`)
- `ui/src/parser/AvauiParser.cpp` (`ParseAnimateBlock`,
  `IsAnimateHeader`, `ParseComponent`/`ParseViewBody` con parámetro
  `animations` adicional)
- `ui/include/avalang/ui/UIModule.h` (comentario fila 18)
- `ui/src/UIModule.cpp` (`AbiVersion() = 18`)
- `ui/CMakeLists.txt` (fuentes + target `ava_animation_demo`)
- `docs/AVALANG_UI_PROGRESS.md` (fila 19)
