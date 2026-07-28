# Plan de implementación: `async`/`await` en AvaLang

> Objetivo final: poder escribir `async func` y usar `await` con semántica similar a C# —
> una función `async` puede suspenderse en cualquier punto de su cuerpo (incluida cualquier
> llamada anidada), y `await` reanuda automáticamente cuando el valor esperado está listo,
> propagando errores igual que una llamada normal.

---

## 0. Diagnóstico (ya hecho)

Estado actual de la VM, verificado directamente sobre el código (`core/src/vm/`):

- El sistema de corrutinas **funciona**: `YieldStmt` → `Compiler::CompileYield` → `OpCode::YIELD`,
  y `resume(co, ...)` vía `VM::Call` hacen swap de `frames_` y devuelven el control correctamente
  cuando la suspensión ocurre en el **frame 0** de la corrutina.
- Existe un flag `is_coroutine_suspended_` (`vm.h:131`) que se pone en `true` dentro de
  `case OpCode::YIELD` (`vm.cpp:1127`) y solo se **consulta una vez**, en el punto de entrada
  de nivel superior (`vm.cpp:325-332`).
- **El bug estructural real**: `case OpCode::CALL` (`vm.cpp:837` en adelante) invoca funciones
  anidadas con `ExecuteFrame(frames_.size() - 1)` de forma **recursiva en C++**, y en ningún
  punto de esas ~5 variantes (Bound, Class/`__init__`, Function) se comprueba
  `is_coroutine_suspended_` tras el `ExecuteFrame` interno. Si un `YIELD` (o el futuro `AWAIT`)
  ocurre dentro de una función llamada desde el cuerpo de la corrutina, ese `nil` de retorno
  se trata como si fuera un valor normal y la ejecución sigue como si nada — la suspensión
  se "pierde" en cuanto cruza un `CALL`.
- `OpCode::RESUME` (bajo nivel, `vm.cpp:1131`) existe pero el compilador nunca lo emite; el
  builtin `resume()` ya cubre ese camino vía `VM::Call`. Es código muerto que conviene limpiar
  o reutilizar, no duplicar.

**Conclusión:** no se puede construir `await` de forma segura *encima* de la arquitectura
actual sin antes resolver la propagación de suspensión a través de llamadas anidadas. Por eso
la Fase 1 no es opcional ni cosmética: es el prerrequisito real de todo lo demás.

---

## Fases

### Fase 1 — Aplanar la propagación de suspensión en la VM
**Por qué:** es el fix de fondo que arregla `yield`/`resume` para el caso general (funciones
anidadas) y que después reutiliza `await` sin duplicar lógica.

- [x] Sustituir la recursión de C++ en `CALL` por una comprobación explícita tras cada
      `ExecuteFrame(frames_.size()-1)`: si `is_coroutine_suspended_` quedó en `true`, el frame
      llamador debe **también** retornar/propagar la suspensión hacia arriba (no seguir
      ejecutando ni sobrescribir el registro de retorno), dejando el `CallFrame` intermedio
      intacto en `frames_` en vez de hacer `pop_back()`.
- [x] Igual para el caso `ValueType::Bound` y para el `__init__` implícito de `Class` — los
      tres puntos de `ExecuteFrame` anidado dentro de `CALL` necesitan el mismo guard.
- [x] Test de regresión: corrutina que hace `yield` dentro de una función auxiliar llamada
      desde el cuerpo del generador (no solo en el nivel superior). Confirmar que
      `resume()` la reanuda correctamente en el punto exacto, con las variables locales del
      frame intermedio intactas.
      → `scripts/test_coroutine_nested_yield.ava`, confirmado 2026-07-27. El guard por sí
      solo alcanzaba para el *primer* `resume()` (recursión de C++ viva); hacía falta además
      `VM::ResumeFromTop()` + `CallFrame::ret_slot` para que el *segundo* `resume()` en
      adelante reanude el frame correcto en vez de `ExecuteFrame(0)`. Ver
      `docs/architecture.md` Bug #1 para el detalle completo.
- [ ] Revisar los otros `ExecuteFrame(frames_.size()-1)` fuera de `CALL` (líneas ~271, ~290,
      ~1011 del `vm_original_2026-07-27_090632.cpp`) por si aplican al mismo patrón (llamadas
      a métodos especiales, operadores sobrecargados, etc.).
- [ ] Confirmar que el mecanismo de excepciones (`RAISE`/`TRY`, ya arreglado en sesión previa)
      sigue funcionando igual una vez tocado `CALL` (no debe interferir con el guard nuevo).

**Riesgo si se salta esta fase:** `await` "funcionará" en demos triviales (todo en el frame 0)
y fallará de forma silenciosa/confusa en cualquier caso real donde el `await` esté dentro de
un helper, un método, o una expresión compuesta.

---

### Fase 2 — Sintaxis y AST: `async func`, `await`
**Por qué:** dar la forma superficial en gramática/AST antes de tocar codegen.

- [ ] Extender la gramática ANTLR4 (`.g4`) con el modificador `async` en declaraciones de
      función/método, y el operador prefijo `await <expr>`.
- [ ] Nuevo nodo AST `AwaitExpr` (expresión, no statement — debe poder usarse en medio de
      cualquier expresión, igual que en C#: `var x = await Foo() + 1;`).
- [ ] Marcar `FuncDecl`/closures con un flag `is_async` (paralelo a como ya se distingue una
      función generadora que usa `yield`).
- [ ] Ava Studio: actualizar el lexer hecho a mano (Code Editor) para resaltar `async` y
      `await` como palabras clave, reusando la paleta de acentos ya definida (naranja
      `#FF7A00` para keywords de control, si es el criterio actual).
- [ ] Autocompletado Tab-triggered: agregar `async`/`await` a la gramática de sugerencias.

---

### Fase 3 — Representación en runtime: `Task`
**Por qué:** `await` necesita algo que esperar. En C# es `Task<T>`; aquí puede ser un nuevo
`ValueType::Task` que envuelve exactamente el mismo objeto `Coroutine` que ya existe — no hace
falta un tipo paralelo, solo una vista con estado (`Pending` / `Completed` / `Faulted`) y un
resultado u error una vez terminado.

- [ ] Nuevo `ValueType::Task` (o reutilizar `Coroutine` con un `has_result`/`error` extra en
      `coroutine.h`) con: `status`, `result`, `error` (para excepciones no capturadas dentro
      del cuerpo async).
- [ ] Llamar a una función `async` **no** ejecuta el cuerpo inmediatamente hasta el primer
      `await` sin más — debe crear y devolver un `Task` (igual que C#: llamar a un método
      `async` siempre devuelve un `Task` de inmediato). Definir explícitamente la semántica de
      *cuándo* arranca a correr el cuerpo (opción simple y predecible: arranca inmediato,
      hasta el primer punto de suspensión — esto es "eager", como C#, a diferencia de
      generadores lazy de otros lenguajes).
- [ ] Reusar el mecanismo de frames de `Coroutine` para el cuerpo del `Task` (mismo swap de
      `frames_` que ya usa `resume()`), apoyándose en el fix de la Fase 1.

---

### Fase 4 — Opcode y compilación de `await`
**Por qué:** conectar la sintaxis de la Fase 2 con el runtime de la Fase 3.

- [ ] Nuevo `OpCode::AWAIT` (o reutilizar `YIELD` con un modo distinto vía operando `c`, a
      decidir según cuánto se quiera compartir código en el intérprete).
- [ ] `Compiler::CompileAwait`: compila la expresión del operando, emite `AWAIT`, y en el
      punto de reanudación coloca el resultado (o relanza la excepción) en el registro
      destino de la expresión `await` — igual que hoy `CompileYield` maneja el registro tras
      un `resume()`.
- [ ] En la VM: `AWAIT` sobre un `Task` ya completado devuelve el valor inmediatamente sin
      suspender (fast path, igual que `await` sobre un `Task.CompletedTask` en C#). Sobre un
      `Task` pendiente, suspende el frame actual (mismo camino que `YIELD`, ahora ya
      propagando bien gracias a la Fase 1) y deja registrado *qué* frame debe reanudarse
      cuando el `Task` esperado termine.
- [ ] Definir el "driver" que reanuda: en ausencia de I/O real, el modelo más simple y
      correcto es que `await` sobre un `Task` que envuelve otra función `async` la ejecute
      de forma síncrona hasta que quede resuelta o vuelva a suspenderse en un `await` más
      profundo — sin loop de eventos todavía. Esto ya da la semántica correcta de C# para
      código 100% CPU-bound / sin I/O, que es el caso de uso inicial pedido.

---

### Fase 5 — Propagación de errores a través de `await`
**Por qué:** pedido explícito de "que funcione realmente como en C#" — una excepción dentro
de una función `async` debe poder capturarse con un `try`/`except` normal alrededor del
`await`, exactamente como una llamada síncrona.

- [ ] Si el cuerpo de un `Task` termina por `RAISE` sin capturar, guardar la excepción en el
      `Task` (`status = Faulted`, `error = <valor>`) en vez de propagarla en el momento —
      igual que C# no lanza hasta que alguien hace `await`/`.Result`.
- [ ] `await` sobre un `Task` en estado `Faulted` debe re-lanzar esa excepción en el punto del
      `await`, integrándose con el mecanismo `TRY`/`RAISE` ya arreglado (reutilizar el patch
      de operando de `TRY` y el pop de handler en `RAISE`, sin tocar su lógica interna).
- [ ] Test: `try { await mayFail() } except e { ... }` capturando un error lanzado varios
      niveles adentro de la función async esperada.

---

### Fase 6 — Scheduler / event loop mínimo (opcional, para I/O real)
**Por qué:** hoy no hay `sleep`, timers, sockets ni I/O no bloqueante. Esta fase es la que
convierte `async/await` de "sintaxis bonita sobre corrutinas síncronas" a concurrencia real.
Se puede posponer sin bloquear el uso pedido (funciones async CPU-bound tipo C# secuencial).

- [ ] Cola de tareas pendientes (microtask queue) en la VM.
- [ ] Builtin `sleep(ms)` que devuelve un `Task` que se resuelve cuando pasa el tiempo,
      integrado con un loop principal que la VM/host debe correr (`ava_run_pending()` o
      similar en la API C).
- [ ] Puntos de extensión para I/O futura (archivos, red) sin comprometerse a implementarlos
      ya.

---

### Fase 7 — Documentación y ejemplos
- [ ] Actualizar el README con la sintaxis real de `async func` / `await`, siguiendo el mismo
      cuidado de verificar contra la gramática que se tuvo en la última reescritura del README.
- [ ] Ejemplos mínimos: función async secuencial, await anidado dentro de un método de clase
      (para validar la Fase 1 en la práctica), y un caso con `try`/`except` alrededor de un
      `await` que falla.
- [ ] Actualizar bindings de interop (C#, Python) si exponen corrutinas/tasks al host.

---

## Resumen de dependencias entre fases

```
Fase 1 (fix propagación en CALL)  ──┐
                                     ├──▶ Fase 4 (AWAIT opcode) ──▶ Fase 5 (errores)
Fase 2 (sintaxis/AST)  ──┐          │
Fase 3 (Task runtime)  ──┴──────────┘
                                     └──▶ Fase 6 (event loop, opcional)
Fase 7 (docs) al final, transversal.
```

La Fase 1 es bloqueante para todo lo demás — es la única que toca código ya en producción
(corrutinas) y por eso conviene aislarla en un commit propio con sus propios tests antes de
tocar gramática o compilador.
