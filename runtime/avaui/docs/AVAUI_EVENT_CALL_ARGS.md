# Eventos con argumentos (`click = Handler(args...)`)

**Status:** ✅ Implementado. Afecta la invocación de handlers, no el
parser ni el formato de archivo -- `.avaui` ya aceptaba esta sintaxis
sin cambios (ver "Por qué no hizo falta tocar el parser" más abajo).

**Archivos modificados:**
- `avahost/src/runtime/runtime_host.cpp` -- `RuntimeHost::InvokeHandler`
- `avastudio/src/design/state_eval.cpp` -- `design::InvokeHandler`

---

## Qué se puede escribir ahora en `view`

Cualquier evento (`click`, `onchange`, etc. -- ver `EventPropNames()`
en `core/src/ui/avaui_text.cpp`) acepta, además del nombre pelado de
siempre, una llamada completa con argumentos:

```
button Guardar
    click = OnAdd(Player)          -- objeto / variable de state
end

text Titulo
    click = OnAdd(Name)            -- variable de state
end

button Saludo
    click = OnAdd("Raul")          -- string literal
end

button Registrar
    click = OnAdd("Raul", 38, data) -- múltiples argumentos, mixtos
end
```

Todo lo que sea una expresión válida de AvaLang funciona como
argumento: identificadores (objetos/variables), strings, números,
booleanos, listas, dicts, e incluso expresiones (`OnAdd(count + 1)`).
Nada de esto es un caso especial nuevo -- es exactamente lo que ya
soporta `argList`/`arg` en `avalang/grammar/AvaLang.g4` para cualquier
llamada AvaLang.

## Cómo lo resuelve la VM

`click = ...` en `.avaui` es texto, no AST. El texto crudo se guarda
sin tocar (`node->SetEvent(key, Value::String(Trim(raw_value)))` en
`avaui_text.cpp` / `PropertyValue` opaco en `AvauiParser.cpp`), y
recién se compila y corre cuando el evento realmente se dispara.
`InvokeHandler` es el único lugar que arma ese texto ejecutable:

```cpp
// handler_name puede ser "OnAdd" (nombre pelado) o "OnAdd(Player)"
// (call completo, tal como se escribió en .avaui)
const std::string source = "__avahost_invoke__ = " + handler_name +
                            (LooksLikeCall(handler_name) ? "" : "()");
```

- **Nombre pelado** (`click = OnAdd`, o un candidato de auto-bind
  como `OnGuardarClick`) → se le agrega `()`: `OnAdd()`.
- **Call completo** (`click = OnAdd(Player)`) → se usa tal cual, sin
  tocar: `OnAdd(Player)`.

`LooksLikeCall(text)` es la única pieza nueva: el texto ya trimeado
termina en `)` y contiene un `(`. No valida la lista de argumentos --
eso lo hace el compilador de AvaLang al compilar `source` (si el call
está mal formado, `ava_compile` devuelve error y `InvokeHandler`
propaga `outError`, igual que cualquier otro error de handler).

El resultado (`OnAdd(Player)`, ya sea nombre pelado + `()` o call
completo) se compila y corre contra la VM activa del request/tab con
`ava_compile` + `ava_run`, exactamente como antes. No hay ninguna
ruta nueva de ejecución: solo cambió qué texto se le pasa al
compilador.

### Resolución de los argumentos (`Player`, `Name`, `data`, ...)

Un identificador usado como argumento (`Player`, `Name`, `data`) se
resuelve como cualquier otra expresión AvaLang: contra los **globals
actuales de la VM** en el momento del click. Eso significa que tiene
que existir ya como global cuando `InvokeHandler` corre -- lo cual ya
está garantizado por el orden de binding que ambos hosts siguen:

1. `BindState(stateJson)` -- publica el bloque `state` como globals.
2. `BindCodeBehind(methodsText)` -- compila el bloque `code`, define
   `func OnAdd(player) ... end` como global invocable.
3. `InvokeHandler(...)` -- recién acá se compila/corre `OnAdd(Player)`.

Si `Player` no es un global válido en ese momento (typo, o una
variable que sólo existe *dentro* de otro handler), `ava_compile`
falla y el error queda en `outError` -- no hay resolución silenciosa
a `nil` salvo que así lo defina el propio lenguaje AvaLang.

```
state
    Player = "Raul"
    Name = "Nombre por defecto"
end

view
    button Guardar
        click = OnAdd(Player, Name)
    end
end

code
    func OnAdd(player, name)
        print(player + " / " + name)
    end
end
```

## Auto-bind sigue siendo zero-arg, a propósito

El binding automático por convención de nombres
(`AutoBindEvents` en `avaui_text.cpp`: `On{PascalId}{PascalEvent}`)
sigue generando **solo nombres pelados** -- nunca inventa argumentos.
Un `button Guardar` con `func OnGuardarClick()` en `code` se sigue
enlazando solo si esa función es realmente cero-arg (o tiene defaults,
ver abajo). Si necesitás pasar argumentos, tenés que escribir el
`click = ...` explícito con el call completo; el auto-bind no cambió.

## Aridad y valores por defecto

AvaLang no valida aridad estricta en la llamada: `EmitDefaultsPrologue`
(`core/src/compiler/compiler.cpp`) completa los parámetros que falten
con su default declarado, o `nil` si no tienen uno. Consecuencias
prácticas:

- Un handler `func OnAdd(player)` invocado como nombre pelado (sin
  auto-bind explícito de argumentos) recibe `player = nil`, no un
  error de compilación.
- Pasar **de más** argumentos que los que el handler declara sí puede
  fallar, según la misma regla del compilador -- probalo si tu handler
  tiene una firma fija.

## Dónde corre esto (los dos hosts)

La misma sintaxis funciona igual en los dos lugares donde AvaHost/
AvaStudio disparan eventos contra una VM. Ninguno de estos puntos de
entrada cambió -- ya pasaban el texto crudo del evento sin tocar, así
que el fix en `InvokeHandler` los cubre a los dos automáticamente:

| Pipeline | Dispara el click vía | Llega a |
|---|---|---|
| AvaHost -- render dinámico (`avaui/` engine) | `ui_vm_event_bridge.cpp` (`EventDispatcher::Click` real, mouse down+up sobre el mismo componente) | `RuntimeHost::InvokeHandler` |
| AvaHost -- render estático (HTML clásico, `core/src/ui`) | POST a `HandleEventRoute` (`data-handler="OnAdd(Player)"` → `EventBinder::ExtractHandlerName`) | `RuntimeHost::InvokeHandler` |
| Ava Studio -- preview del Designer | Ctrl+Click sobre un nodo en `designer_canvas.cpp` | `design::InvokeHandler` |

El transporte HTML del pipeline estático ya escapaba/decodificaba
correctamente paréntesis, comillas y comas (`HtmlEscapeAttr` +
`ParseQueryString`), así que un `data-handler="OnAdd(&quot;Raul&quot;)"`
viaja intacto ida y vuelta sin cambios adicionales.

## Por qué no hizo falta tocar el parser ni el formato de archivo

Tres capas ya estaban listas antes de este fix y no se tocaron:

1. **Gramática/VM de AvaLang** (`avalang/grammar/AvaLang.g4`,
   `postfix`/`trailer`/`callTrailer`/`argList`/`arg`) ya soporta
   llamadas con múltiples argumentos posicionales de cualquier tipo.
2. **Los dos parsers `.avaui`** (`core/src/ui/avaui_text.cpp` línea
   `SetEvent(key, Value::String(Trim(raw_value)))`, y
   `avaui/src/parser/AvauiParser.cpp`'s `InferValue`) ya guardan el
   texto del evento tal cual se escribió, paréntesis incluidos --
   nunca lo truncaban ni lo interpretaban.
3. **El transporte HTML/POST** del pipeline estático ya era seguro
   para texto arbitrario.

El único punto que asumía "esto siempre es un nombre pelado" era
`InvokeHandler` (por duplicado, en los dos hosts) -- ver el diff de
este cambio.

## Limitaciones conocidas

- `LooksLikeCall` es una heurística de texto (termina en `)`, tiene
  `(`), no un parser -- confía en que AvaLang rechace algo mal
  formado en tiempo de compilación. Suficiente hoy porque
  `IsComponentCall` (import de componentes, `AvauiParser.cpp`) usa el
  mismo criterio.
- Eventos distintos de `click` (`onchange`, `oninput`, ...) hoy solo
  se resuelven de verdad en el pipeline estático (`HandleEventRoute`
  procesa cualquier `data-event`/`data-handler`, sin importar el
  nombre del evento) -- `ui_vm_event_bridge.cpp` (pipeline dinámico)
  todavía solo cablea `click` de verdad contra `IEventDispatcher`.
  Nada de esto cambió con este fix; se documenta acá porque es
  relevante para saber dónde probar un `onchange = Handler(args)`.
- No hay chequeo de aridad "estricto" en tiempo de escritura -- un
  typo en el nombre de un argumento (`OnAdd(Plaeyr)`) recién se nota
  al hacer click (o al compilar el `code` block, si ese identificador
  tampoco existe como global en ningún lado).
