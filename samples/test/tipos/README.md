# Suite de pruebas mínima — Sistema de Tipos

Fixtures `.ava` para las secciones 22 ("Errores que deben existir") y 23
("Suite de pruebas mínima") de `AvaLang_Plan_Sistema_de_Tipos.md`, una vez
cerradas las 16 fases de implementación (`AvaLang_Plan_Sistema_de_Tipos_PROGRESO.md`).

Convención: `ok_*.ava` debe compilar limpio (exit 0); `err_*.ava` debe
fallar la compilación con un `AvaError` concreto (no un crash, no un
segundo error distinto al listado).

## `ok_*.ava` — formas válidas

| Archivo | Cubre |
|---|---|
| `ok_variables.ava` | Sección 23 "Variables": inferencia sin anotar + anotación explícita para los 3 primitivos no-numéricos/numérico (`int`, `string`, `bool`). |
| `ok_inferencia.ava` | Sección 23 "Inferencia": `x = 10` → `int`, `y = x + 20` → `int` (Fase 5/11). |
| `ok_funciones.ava` | Sección 23 "Funciones"/"Llamadas" (caso válido): parámetros y retorno anotados, llamada con tipos correctos (Fases 8/9/10). |
| `ok_clases.ava` | Fase 13: herencia, override de método, campo tipado, `pet.speak()` inferido vía `class_method_returns_`. |
| `ok_colecciones.ava` | Fase 15: literal de lista/dict, index, slice — todos con tipo de elemento inferido. |
| `ok_extern.ava` | Fase 16: `extern` con parámetro y retorno anotados; `K.GetTickCount()` infiere `int`. |

## `err_*.ava` — errores que deben existir (sección 22)

| Archivo | Error esperado | Fase que lo detecta |
|---|---|---|
| `err_tipo_incompatible.ava` | `age as int = "hello"` → tipo incompatible | Fase 6 |
| `err_asignacion_incompatible.ava` | reasignar `int` con `string` | Fase 7 |
| `err_argumento_incorrecto.ava` | `add("hello")` contra `func add(a as int)` | Fase 9 |
| `err_retorno_incorrecto.ava` | `return "hello"` en `func ... as int` | Fase 10 |
| `err_operacion_incompatible.ava` | `true + 5` → operando `bool` inválido | Fase 11 |
| `err_extern_argumento_incorrecto.ava` | `K.Sleep("cinco")` contra `func Sleep(ms as int)` | Fase 16 |

### Nota sobre `err_operacion_incompatible.ava`

La sección 22 del plan da `true + "hello"` como ejemplo de "operación
incompatible". La Fase 11 (ver su entrada en `PROGRESO.md`, "Decisión de
fondo") documentó explícitamente que **ese ejemplo puntual no es un error
real** en esta implementación: `+` con un `string` a cualquier lado
siempre concatena en la VM (`"truehello"`), `bool` incluido, así que
marcarlo como error del compilador sería un falso positivo sobre código
que hoy corre bien. Este fixture usa en cambio `true + 5` (ningún lado es
`string`, `bool` sí revienta `CoerceToNumber`), que es el caso que
`ValidateBinOpTypes` sí marca — mismo operador, mismo tipo de violación
("operando `bool` en aritmética"), solo con el segundo operando cambiado
para que dispare el error real en vez del que el plan asumía sin haber
auditado la VM.

## Limitación conocida de este entorno

Igual que documenta cada fase desde la 8 (`PROGRESO.md`), este entorno no
tiene forma de compilar y **enlazar** `ava_cli`/`avalang` de punta a
punta (desajuste entre el runtime ANTLR C++ instalable acá, 4.10, y el
generado por el jar 4.13.2 del repo). Estos fixtures no se ejecutaron
contra un binario real; son la suite lista para correr con
`ava_cli run samples/test/tipos/<archivo>.ava` en un entorno con ese
desajuste resuelto (ver `scripts/build_cli.sh`/`install.sh`). Cada
`err_*.ava` fue, sin embargo, verificado a mano línea por línea contra la
lógica real de `compiler.cpp` (qué rama de `ValidateTypeAnnotation`/
`ValidateReassignment`/`CheckCallArgs`/`CheckReturnType`/
`ValidateBinOpTypes`/`CheckMethodCallArgs` dispara, y con qué mensaje),
no solo copiado del plan.
