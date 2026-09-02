# Diseño: consistencia de listas e índices en AvaLang (VB6 modernizado)

Contexto: durante el testeo de contenedores (Ago 2026) se encontraron dos
bugs reales de comportamiento y una mezcla de estilos sin criterio
explícito documentado. Este archivo fija el criterio para que sesiones
futuras no vuelvan a dudar qué convención sigue AvaLang.

## Principio general

AvaLang parte de VB6 modernizado: sintaxis y semántica clásicas de VB6
donde VB6 ya tiene una solución razonable, adaptadas/extendidas al estilo
moderno (Python/JS) SOLO donde VB6 no ofrece nada equivalente. La regla
para decidir un caso dudoso es esa, en ese orden — no "lo que sea más
corto de escribir" ni "lo que ya esté de moda".

## 1. Por qué conviven `for i = 1 to 3` y `lista[1:3]` (no es un descuido)

- `for i = 1 to N [step S] ... end` es VB6 puro (rango inclusive con
  step) — se mantiene tal cual porque VB6 ya resuelve bien ese caso.
- `lista[1:3]` (slice) y `lista[-1]` (índice negativo desde el final) son
  Python, adoptados porque **VB6 no tiene sintaxis de slicing ni de
  índice negativo** — no hay nada VB6 que modernizar ahí, así que se
  importa la convención más establecida en lenguajes modernos en vez de
  inventar una propia.
- Índices 0-based: coincide con VB6 clásico por default (salvo `Option
  Base 1`, que AvaLang no adopta), así que tampoco es una ruptura real
  con la base VB6.

Decisión: **se documenta la convivencia como intencional**, no se
unifica `to/step` y `:` bajo una sola sintaxis. Si en el futuro se agrega
azúcar sintáctica de rango reusable (ej. `lista[1 to 3]` como alias de
`lista[1:3]`), evaluarlo aparte — no bloqueante para ningún bug real.

## 2. Política de errores de índice: dura y uniforme (estilo VB6
   "Subscript out of range", con detalle moderno)

VB6 clásico tira `"Subscript out of range"` de forma **uniforme** para
cualquier índice inválido, sin importar si es negativo, no entero, o
simplemente demasiado grande. Antes de este fix, AvaLang tenía tres
comportamientos distintos para tres formas del mismo error:

| Caso | Antes | Ahora |
|---|---|---|
| Índice no entero (`lst[1.5]`) | excepción dura | excepción dura (sin cambio) |
| Índice negativo fuera de rango (`lst[-10]` en lista de 3) | excepción dura | excepción dura (sin cambio) |
| Índice positivo fuera de rango, **lectura** (`lst[10]`) | `nil` silencioso | excepción dura |
| Índice positivo fuera de rango, **escritura** (`lst[10] = 99`) | no-op silencioso, dato perdido sin aviso | excepción dura |

Decisión: se lleva todo a excepción dura y uniforme (`ValidateIntegerIndex`
en `vm_helpers.cpp/.h`, ahora recibe el largo del contenedor y valida
también el límite superior), aplicando el mismo criterio de
`vm_containers.cpp::OpGetIndex`/`OpSetIndex` para `list` y `string` tanto
en lectura como en escritura. El mensaje moderniza el clásico
"Subscript out of range" con el índice y el largo real
(`"list index: index out of range: 10 (length 3)"`), en vez del mensaje
genérico de VB6, para que el error sea accionable sin tener que
adivinar el estado del contenedor.

No se tocó `list.removeAt(i)` (builtin, no pasa por `OpGetIndex`/
`OpSetIndex`): ese método ya documenta su propio contrato de "intentar
remover, devolver nil si no existe" (patrón explícito tipo
`TryRemove`), que es un caso de uso distinto al operador `[]` — se deja
fuera de este cambio a propósito.

## 3. Igualdad estructural en `list.contains()` / `list.remove()`

Bug real encontrado: estos dos métodos comparaban `item.as.n ==
args[1].as.n` — el campo numérico crudo del union `ava_value_t.as`, sin
mirar el tipo. Para `string`/instancia eso compara punteros/bits
internos, no contenido, así que solo "funcionaba" cuando el string era
literalmente el mismo objeto en memoria (deduplicación de constantes del
compilador), y fallaba en silencio con strings construidos en runtime —
divergiendo de lo que el propio operador `==` del lenguaje sí resuelve
bien (`vm_helpers.cpp::ValueEquals`, comparación estructural).

Decisión: `contains()`/`remove()` ahora usan `ValueEquals` sobre el
`Value` interno (vía `FromC`), la misma función que ya usa `==` —
regla general para cualquier builtin nuevo que compare valores de
AvaLang: nunca comparar el union `ava_value_t.as` a mano, siempre pasar
por `ValueEquals`.

## 4. Pendiente, no resuelto en este pase (anotado para sesión futura)

Superficie de API asimétrica entre `str`/`list`/`dict` (ver hallazgo
original): falta `list.indexOf`, `list.sort`, `list.reverse`,
`list.join`, `list.slice`, `list.clear`, `list.copy` — existen en `str`
pero no en `list`. Prioridad baja frente a los bugs de datos de arriba;
no implementado en este pase.
