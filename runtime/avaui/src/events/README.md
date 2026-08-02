# Phase 5 -- Event System

Manejo de entrada desde PAL (mouse, teclado, etc.) y despacho de eventos a través del árbol de componentes.

## Interfaces Públicas

- **`IEvent`** (`avalang/ui/events/IEvent.h`)
  - Base para todos los eventos.
  - `EventType` y `MouseButton` enums.
  - Métodos: `Type()`, `Target()`, `SetTarget()`, `IsPropagating()`, `StopPropagation()`, `PreventDefault()`, `Timestamp()`.

- **`IMouseEvent`** : `IEvent`
  - Extender base con datos específicos del ratón.
  - `Button()`, `X()`, `Y()`, `DeltaX()`, `DeltaY()`.

- **`IKeyboardEvent`** : `IEvent`
  - Extender base con datos específicos de teclado.
  - `KeyCode()`, `IsShiftDown()`, `IsCtrlDown()`, `IsAltDown()`, `IsMetaDown()`.

- **`IEventHandler`**
  - Interfaz para observadores: `OnEvent(IEvent*)`.
  - Implementada por los observadores; se llama cuando ocurre un evento.

- **`IEventDispatcher`** (`avalang/ui/events/IEventDispatcher.h`)
  - Punto central de despacho.
  - `Create()` factory.
  - `Subscribe(target, type, handler)` → id; `Unsubscribe(id)`.
  - `Dispatch(event)` con propagación automática.
  - `PollInput(root)` — llamar una vez por frame antes de renderizar.
  - Focus: `FocusedComponent()`, `SetFocusedComponent()`.
  - Estado del ratón: `MouseX()`, `MouseY()`, `IsMouseButtonDown()`.

## Implementación Interna

- **`Event` / `MouseEvent` / `KeyboardEvent`** (`Event.h/.cpp`)
  - Implementaciones concretas.
  - Timestamp automático desde época.

- **`EventDispatcher`** (`EventDispatcher.h/.cpp`)
  - Colecciona suscriptores (map de componentes + tipos).
  - `PollInput()` : obtiene estado desde PAL (`IMouse`, `IKeyboard`).
  - Emite eventos diferenciales (cambios entre frames).
  - `HitTest()` : encuentra qué componente está bajo las coordenadas del ratón.
  - `Dispatch()` : entrega evento a suscriptores + gestiona propagación.

## Flujo Típico

1. **Inicialización** (p. ej., en UIModule):
   ```cpp
   auto dispatcher = IEventDispatcher::Create();
   dispatcher->SetLayoutEngine(layoutEngine);
   auto handlerId = dispatcher->Subscribe(targetComponentId, EventType::Click, handler);
   ```

2. **Cada frame**:
   ```cpp
   dispatcher->PollInput(rootComponent);  // Convierte entrada PAL en eventos
   dispatcher->Dispatch(event);            // Distribuye a suscriptores
   ```

3. **Handlers**:
   ```cpp
   class MyHandler : public IEventHandler {
       void OnEvent(IEvent* event) override {
           if (auto mouseEvent = dynamic_cast<IMouseEvent*>(event)) {
               // Handle click, etc.
               event->StopPropagation();  // Evitar que suba más
           }
       }
   };
   ```

## Limitaciones / Pendientes

- **Fase 5 actual** : polling de entrada bruto (Estado actual de botones/teclas).
  - Futuro: event records / key press síncrono desde window message loop (Win32).
  - Futuro: multiples ventanas / viewport local coordinates.

- **Hit-testing** : simplificado, solo usa LayoutRect bounds.
  - Futuro: respeto de clipping, opacity, pointer-events CSS.

- **Focus** : almacenado pero sin reacciones visuales aún.
  - Futuro: enganchar con Render Tree para hilight, etc.

- **Bubbling** : soportado en struct, no implementado en Dispatch todavía.
  - Pendiente (Fase 6+): integración con árbol padre-hijo.

## Interoperabilidad con otras Fases

- **→ Components (Phase 2)** : cada componente puede tener observadores vía dispatcher.
- **→ Layout (Phase 3)** : dispatcher usa LayoutEngine para hit-testing.
- **→ State (Phase 4)** : eventos pueden mutar estado (p. ej., click binding → state update).
- **← Platform (Phase 0)** : IMouse, IKeyboard polling.
