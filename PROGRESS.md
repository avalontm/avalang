# AvaLang — Estado del Desarrollo

Lenguaje de scripting embebible con bloques `then`/`do`/`end`
(estilo Visual Basic/Lua) y semántica dinámica tipo Lua. Bytecode VM
register-based, con clases, closures, coroutines y excepciones.

> Este documento reemplaza la versión anterior de `PROGRESS.md`, que
> tenía la misma información repetida en 3-4 lugares distintos
> (sección "Estado Actual", tabla de opcodes, y cada "Sesión..."). Acá
> cada cosa vive en un solo lugar.

---

## Tabla de Estado General

| Área | Estado | Notas |
|---|:---:|---|
| **Tipos primitivos** (nil, bool, number, string) | ✅ | Verificado — Test 01 |
| **Listas** (crear, indexar, slicing `arr[a:b:c]`) | ✅ | Verificado — Test 04 |
| **Diccionarios** | ✅ | Verificado — Test 05 |
| **Strings** + interpolación f-string (`$"...{x}..."`) | ✅ | Verificado — Test 06 |
| **Operadores** (aritméticos, comparación, lógicos) | ✅ | `/` ahora es división normal (float), `floor()` para truncar |
| **Control flow** (if/elif/else, while, for, break, continue) | ✅ | Verificado — Test 02 |
| **Funciones** (params, return, recursión) | ✅ | Verificado — Test 03 |
| **Closures** (funciones anidadas, upvalues) | 🚧 | Verificado — Test 08. **Bug:** no mantienen estado correctamente. Ver [Bugs Conocidos](#bugs-conocidos-jul-2026). |
| **Clases y objetos** (`init`, métodos, atributos) | 🚧 | Verificado — Test 07. **Bug:** props no reciben parámetros del constructor. Ver [Bugs Conocidos](#bugs-conocidos-jul-2026). |
| **Herencia** (`class Child : Parent`, `base()`) | ✅ | Corregida y funcional |
| **Excepciones** (try/catch/finally, raise) | ✅ | Verificado — Test 09 |
| **Coroutines** (`coroutine()`, `resume()`, `yield`) — caso directo | ✅ | Verificado — Test 10 |
| **Coroutines** — `yield` en llamada anidada | ❌ | Solo desenrolla un nivel, como `return` normal — no propaga hasta arriba |
| **Coroutines** — `resume()` sobre coroutine `Dead` | ❌ | No lanza error, devuelve `nil` en silencio (debería validar) |
| **Módulos / import** (con alias `as`) | ✅ | — |
| **Mensajes de error** (línea, columna, visualización de código) | ✅ | — |
| **Built-ins** (`type`, `str`, `int`, `float`, `abs`, `round`, `floor`, `ceil`, `min`, `max`, `pow`, `sqrt`, `sum`, `sorted`, `reversed`, `any`, `all`, `len`, `range`) | ✅ | — |
| **String methods** (`.upper()`, `.split()`, `.trim()`, ...) | ✅ | — |
| **List methods** (`.append()`, `.pop()`, `.insert()`, ...) | ✅ | — |
| **Dict methods** (`.keys()`, `.values()`, `.items()`, ...) | ✅ | — |
| Decorators | ❌ | No implementado |
| List/dict comprehensions | ❌ | No implementado |
| C# Interop (llamadas bidireccionales script ↔ C#) | ✅ | Ver [Bindings](#bindings--frameworks) |
| AvaLang.UI (framework de componentes C# + HTML renderer) | ✅ | Ver [Bindings](#bindings--frameworks) |

**Leyenda:** ✅ implementado y verificado · 🚧 implementado con bug(s)
conocido(s) · ❌ no implementado / pendiente

---

## Últimos Tests Manuales

10 tests completados sobre las áreas core del lenguaje:

| # | Test | Resultado |
|---|---|:---:|
| 01 | Primitives | ✅ |
| 02 | Control Flow | ✅ |
| 03 | Functions | ✅ |
| 04 | Lists | ✅ |
| 05 | Dictionaries | ✅ |
| 06 | Strings | ✅ |
| 07 | Classes | 🚧 bug |
| 08 | Closures | 🚧 bug |
| 09 | Exceptions | ✅ |
| 10 | Coroutines | ✅ |

### Bugs Conocidos (Jul 2026)

1. **Props de clase no reciben parámetros en constructor.**
   No confirmado aún si es un bug real del compilador o un problema de
   sintaxis del script de prueba: la documentación histórica de este
   proyecto (y ejemplos previos de este mismo archivo) usaban el patrón
   `class Point(x, y)` con parámetros en el header de la clase y
   `__init__(self, x, y)` — ese patrón está **explícitamente rechazado**
   en `docs/SYNTAX_DESIGN.md` (el header de clase solo lleva el padre,
   nunca parámetros). La forma vigente es:
   ```ava
   class Point
       func init(x, y)
           this.x = x
           this.y = y
       end
   end
   ```
   Antes de tratar esto como bug de compilador, vale la pena confirmar
   que el script de prueba usa esta forma y no la vieja.

2. **Closures no mantienen estado correctamente.** Contradice el
   `✅ Funcional` que tenía la versión anterior de este documento y el
   trabajo de upvalues (`GETUPVAL`/`SETUPVAL`) de sesiones previas —
   posible regresión. Sin script de repro todavía; agregar uno a
   `scripts/` en cuanto se aísle el caso.

---

## Arquitectura Implementada

**VM register-based** — acceso a frames por índice (evita dangling
references), bound methods para métodos de instancia, herencia vía
`base()`.

**Value model** — tag-union (`Nil`, `Bool`, `Number`, `String`, `List`,
`Dict`, `Function`, `Native`, `Instance`, `Class`, `Coroutine`,
`Bound`), reference counting, conversión a/desde la C API
(`ava_value_t`).

**Compilador** — AST → bytecode (registros virtuales), clases con
methods/attrs/`init`, expresiones (`BinOp`, `UnOp`, `Call`, `Index`,
`Attr`, `List`, `Dict`), f-strings compiladas con interpolación,
control flow completo.

### Opcodes

34 opcodes, formato `iABC` (operandos de ancho fijo), tabla de
constantes (K) para números/strings/clases, `CallFrame`s con registros
virtuales.

| Opcode | Descripción | Estado |
|---|---|:---:|
| `LOADK`, `LOADNIL`, `LOADBOOL`, `MOVE` | Carga de valores | ✅ |
| `GETGLOBAL`, `SETGLOBAL` | Variables globales | ✅ |
| `GETINDEX`, `SETINDEX` | Indexación lista/dict | ✅ |
| `GETATTR`, `SETATTR` | Atributos de objeto | ✅ |
| `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `POW` | Aritmética | ✅ |
| `NEG`, `NOT` | Unarios | ✅ |
| `EQ`, `NE`, `LT`, `LE`, `GT`, `GE` | Comparación | ✅ |
| `JMP`, `TEST` | Saltos | ✅ |
| `CALL`, `RETURN` | Llamadas a función | ✅ |
| `CLOSURE` | Creación de closures | ✅ |
| `NEWLIST`, `LISTAPPEND` | Listas | ✅ |
| `NEWDICT` | Diccionarios | ✅ |
| `NEWCLASS`, `NEWINSTANCE` | Clases e instancias | ✅ |
| `BASECALL` | Llamadas a método base | ✅ |
| `TRY`, `TRY_END`, `CATCH`, `RAISE` | Excepciones | ✅ |
| `GETUPVAL`, `SETUPVAL` | Upvalues | ✅ (ver bug de closures arriba) |
| `YIELD` | Coroutines: suspender | ✅ |
| `CALL` (a `coroutine`/`resume` nativos) | Coroutines: crear/reanudar | ✅ |
| `RESUME` (opcode) | No usado — `resume()` se compila como `CALL`, ver `docs/coroutines.md` | ❌ código muerto |
| `FOR`, `FORPREP` | Iteración | ❌ implementado a nivel compiler, sin opcode dedicado |

---

## Bindings & Frameworks

Capa opcional sobre el core — no forma parte del lenguaje en sí, sino
de los bindings que consumen la C API (`include/ava.h`).

### C# Interop — ✅ Funcional

Llamadas bidireccionales script ↔ C# verificadas: C# puede cargar y
ejecutar scripts, llamar funciones definidas en AvaLang
(`ava.Call<T>(...)`), leer/escribir variables globales, y registrar
funciones y objetos de C# invocables desde el script. Ver
`bindings/csharp/BINDING_CSHARP_PROGRESS.md` para la referencia
completa de la API.

### AvaLang.UI — ✅ Completado

Framework de componentes puro C# (sin dependencia de la DLL nativa)
más un renderer a HTML5/CSS, siguiendo el Component Tree definido en
`docs/architecture/01_COMPONENT_TREE_AND_DSL.md`.

```
bindings/csharp/
├── AvaLang.Interop/        # Interop con VM nativo
├── AvaLang.UI/              # Framework UI puro C#
│   ├── Core/                # Component, ComponentTree, LayoutType
│   ├── Rendering/           # HtmlRenderer, IComponentRenderer
│   └── Native/               # Implementación in-memory (stub)
├── AvaLang.UI.Host/         # AvaHost, ScriptRuntime (script → ComponentTree)
└── AvaLang.UI.Demo/         # Demo
```

| Componente | Descripción |
|---|---|
| `Column`, `Row`, `Stack`, `Grid`, `Flex` | Layout |
| `Text`, `Image`, `Spacer`, `Divider`, `Link` | Contenido |
| `Button`, `TextBox`, `CheckBox`, `RadioButton` | Interactivo |

Tests de integración pasando (`HtmlRenderer`, creación/jerarquía de
componentes, propiedades, render de página completa) — ver
`dotnet run --project AvaLang.Tests/AvaLang.Tests.csproj`.

---

## Historial de Debugging Relevante

Bugs no triviales ya resueltos, dejados acá como referencia rápida en
vez del relato completo de cada sesión:

- **continue en loops:**
  1. Offset de JMP insuficiente: JMP usaba 16 bits (`int16_t`) que causaba overflow en loops grandes.
  2. Cálculo incorrecto: `PatchJump` calculaba offsets hacia adelante, pero `continue` necesita saltar hacia atrás.
  Fix:
  - Extendido JMP para usar offsets de 32 bits (`bx32` en `opcodes.h`)
  - Agregado `PatchContinueJump()` para offsets negativos (`compiler.cpp:69-74`)
  - Actualizado `CompileWhile`, `CompileForList`, `CompileForCoroutine` para usar `PatchContinueJump`
- **Coroutines (4 bugs, resueltos en orden):**
  1. `coroutine`/`resume` registrados con `RegisterBuiltinMethod` (solo
     para azúcar `obj.metodo()`) en vez de `RegisterNative` → no
     resolvían como funciones globales.
  2. `YIELD` mezclaba el stack de frames del caller con el de la
     coroutine → corrupción. Fix: `YIELD` ahora solo hace `return`, y
     `VM::Call` es el único dueño del swap de frames.
  3. `Coroutine` marcado como ref-counted sin heredar de `Object` →
     `Release()` corrompía memoria. Sacado de `IsRefCounted()`.
  4. `YieldStmt` faltaba en la lista de `std::any_cast` de
     `AstBuilder::stmtFromAny()` → el nodo se descartaba en silencio y
     el `yield` nunca se compilaba.
- **Slicing:** el compilador no avanzaba `next_reg_` al escribir
  `LOADNIL` para argumentos faltantes en slices → `CompileExpr`
  subsiguiente sobrescribía el nil. Fix: reservar 4 slots con
  `AllocReg()` explícito antes de compilar subexpresiones.
- **`native_slice`:** off-by-one en umbrales de `count`, y `end=0`
  válido se confundía con "sin `end`". Fix: flag `has_end` explícito.
- **AugAssign en atributos** (`this.attr += 1`): retornaba `nil` antes
  del fix.
- **Keyword `self`:** ahora reconocido como alias de `this` en el
  compilador.

Limitación conocida sin resolver: `yield` dentro de una llamada
anidada (no en el cuerpo directo de la coroutine) no propaga la
suspensión hasta arriba — se comporta como un `return` normal, solo
desenrolla un nivel. Ver `docs/coroutines.md`.

---

## Estructura del Proyecto

```
avalang/
├── include/ava.h            # API pública C
├── grammar/AvaLang.g4       # Gramática ANTLR4
├── src/
│   ├── ast/                 # Nodos AST + Parser → AST
│   ├── compiler/            # AST → bytecode
│   ├── frontend/             # frontend_antlr.cpp / frontend_stub.cpp
│   ├── vm/                   # VM, Value/Objects, opcodes, Proto/Closure, coroutine
│   ├── api/                  # c_api.cpp
│   └── cli/                  # ava_cli
├── scripts/                  # Scripts de prueba .ava
├── bindings/csharp/          # Interop + AvaLang.UI
└── docs/                     # Referencia del lenguaje + docs/architecture/
```

## Scripts de Prueba (Jul 2026)

```bash
# Windows
build\Release\ava_cli.exe scripts\<test>.ava
```

### Resultados de Tests en scripts/ (Jul 2026)

| Script | Descripción | Resultado |
|---|---|:---:|
| `test_variables.ava` | nil, bool, numbers, strings, fstrings, list, dict, shadow, multi-assign | ✅ |
| `test_operators.ava` | Aritmética, comparación, lógicos, inc/dec, augmented assign | ✅ |
| `test_if.ava` | if/elif/else, condiciones | ✅ |
| `test_while.ava` | while, break, continue, Fibonacci, nested while | ✅ Todos pasan |
| `test_simple2.ava` | if simple | ✅ |
| `debug_continue.ava` | continue skip numbers | ✅ |
| `debug_continue2.ava` | continue vs if | ✅ |
| `test_continue_large.ava` | continue en loop grande (100 iteraciones) | ✅ |
| `test_assignments.ava` | Asignaciones normales y múltiples con comas | ✅ |

### Algoritmos Verificados (Jul 2026)

| Script | Descripción | Resultado |
|---|---|:---:|
| `test_binary_search.ava` | Búsqueda binaria con `floor((low+high)/2)` | ✅ |
| `test_bubble_sort.ava` | Ordenamiento burbuja | ✅ |
| `test_div_normal.ava` | División normal: `7/2 = 3.5` | ✅ |

**Nota:** `/` ahora es división real (float). Para obtener enteros (índices de array),
usar `floor(x)` o `int(x)`.

### Nueva Funcionalidad: Asignación Múltiple en una línea

**Sintaxis:** `a = 1, b = 2, c = 3`

Permite declarar múltiples variables en una sola línea separadas por comas. Equivale a:
```lua
a = 1
b = 2
c = 3
```

**Ejemplos válidos:**
```lua
x = 1, y = 2, z = 3
i=1,j=2,k=3
p = 100
q = 200, r = 300
```

**Implementación:**
- **Gramatica** (`AvaLang.g4`): Nueva regla `multiAssignStatement`
- **AST Builder** (`ast_builder.cpp`): Nuevo visitor `visitMultiAssignStatement`
- No requiere cambios en el VM ni en el compilador

---

## Scripts de Prueba (Original)

## Build

```bash
# Windows
build.bat              # Release (lib + ava_cli.exe)
build.bat debug
build.bat ninja

# Linux/macOS
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Manejo de Errores de Sintaxis

```
error at script.ava:5:12: missing 'then' at '\n    '
    5 | if x > 0
              ^
```

Línea y columna exactas, mensaje descriptivo, y visualización del
código con caret debajo del token ofensivo.

---

**Fecha:** 2026-07-24
**Estado:** Desarrollo activo — funcionalidades core completas, 2 bugs
abiertos (closures, constructor de clases — ver arriba).

### División Normal con `/` (Jul 2026)

**Cambio:** El operador `/` ahora es división normal (float), más intuitivo que `//` para Python.

**Antes:**
```lua
mid = (low + high) // 2  # integer division
```

**Ahora:**
```lua
mid = floor((low + high) / 2)  # usar floor() si necesitas entero
```

**Builtins disponibles para truncar:**
- `floor(x)` → mayor entero ≤ x
- `int(x)` → truncar decimal
- `round(x)` → redondear
