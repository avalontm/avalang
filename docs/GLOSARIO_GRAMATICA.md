# Glosario de la gramática de AvaLang

Referencia rápida de la sintaxis de **AvaLang**, generada directamente a partir de
[`runtime/avalang/grammar/AvaLang.g4`](../runtime/avalang/grammar/AvaLang.g4).

> **Alcance de este documento.** Todo lo de las secciones 1 a 8 sale exclusivamente del `.g4`:
> es la sintaxis del lenguaje tal cual la reconoce el parser, y no debería quedar desactualizada
> mientras no cambie la gramática. La sección 9 ("Funciones incorporadas") es distinta: esos
> nombres **no están definidos en la gramática** (para el parser, `print(x)` es una llamada
> cualquiera a algo llamado `print`) — los junté observando el código real en `samples/` y
> `tools/vscode/examples/example.ava`, así que puede no ser una lista completa. La semántica de
> tipos (qué combinaciones de tipos son válidas al asignar, comparar, etc.) tampoco vive en el
> `.g4`: la valida el compilador en tiempo de compilación, no el parser.

## Índice

1. [Estructura de un programa](#1-estructura-de-un-programa)
2. [Variables, asignación y tipos](#2-variables-asignación-y-tipos)
3. [Operadores](#3-operadores)
4. [Control de flujo](#4-control-de-flujo)
5. [Funciones y lambdas](#5-funciones-y-lambdas)
6. [Clases](#6-clases)
7. [Manejo de errores](#7-manejo-de-errores)
8. [Literales, strings y colecciones](#8-literales-strings-y-colecciones)
9. [Módulos, `extern` y funciones incorporadas](#9-módulos-extern-y-funciones-incorporadas)
10. [Comentarios y fin de línea](#10-comentarios-y-fin-de-línea)

---

## 1. Estructura de un programa

Un archivo `.ava` es una secuencia de **statements** (sentencias). Cada sentencia es *simple*
(termina en salto de línea o `;`) o *compuesta* (abre un bloque y cierra con `end`).

```
statement
├── simpleStatement     (asignaciones, return, break, continue, pass, import, raise, ++/--, ...)
└── compoundStatement   (if, while, for, func, class, try, extern, select, async func)
```

No hay una sentencia `main`/punto de entrada especial: el archivo se ejecuta de arriba a
abajo, statement por statement. Si querés un punto de entrada explícito, definís una función y
la llamás al final del archivo (como hace `main()` en el ejemplo de async más abajo).

Una expresión sola (por ejemplo, una llamada cuyo resultado no te interesa guardar) es en sí
misma un statement válido (`exprStatement`):

```ava
print("hola")   # llamada suelta, sin asignar el resultado a nada
1 + 1            # sintácticamente válido aunque no tenga efecto
```

**Nota sobre el token `ava`.** La gramática define un token léxico `AVA_LANG` para la palabra
`'ava'`, pero ninguna regla del parser lo usa — no aparece en ningún statement ni expresión.
Al día de hoy parece un token reservado sin uso activo; si intentás usar `ava` como nombre de
variable, el lexer probablemente lo tokenice distinto a un `NAME` normal, aunque esto no está
confirmado contra el compilador (el `.g4` solo define el token, no qué hace el resto del
pipeline con él).

---

## 2. Variables, asignación y tipos

### Asignación simple

```ava
x = 10
```

No hace falta declarar nada antes: la primera asignación crea la variable (el tipo se infiere).

### Asignación múltiple

```ava
a, b = 1, 2
```

### Asignación aumentada (`op=`)

```ava
x += 1      # también -=  *=  /=  %=  //=
```

### Incremento / decremento

```ava
x++
x--
++x       # también existe la forma prefija (equivalente en la gramática, ver `unary`)
--x
```

### Asignar a índice o atributo

`target` no es solo un nombre: puede llevar índices/atributos encadenados a la izquierda del `=`.

```ava
items[0] = "nuevo valor"
obj.atributo = 10
matriz[0][1] = 5
persona.direccion.calle = "Falsa 123"
```

### Anotación de tipo (`as Type`)

`as` después de una variable **anota** el tipo, no hace un cast. Hay dos formas:

```ava
edad as int = 25          # declaración + inicialización con tipo
edad as int               # declaración sin inicializar (typedDeclStatement)
```

Los cuatro tipos primitivos son `int`, `float`, `bool`, `string`. Cualquier otro nombre ahí
(`edad as Persona`) se trata como un tipo de clase de usuario.

```ava
local mensaje = "hola"          # `local` fuerza una variable nueva en el scope actual
local edad as int = 30          # también admite tipo
```

`local` sirve para *shadowear* una variable con el mismo nombre en un scope más externo en vez
de reasignarla.

### Modificadores de miembro (`static` / `private`)

Solo tienen sentido dentro de una clase (lo valida el compilador, no el parser):

```ava
class Contador
    static total = 0          # compartido entre todas las instancias
    private secreto = 100     # no accesible desde fuera de la clase
    static private ambos = 1  # se pueden combinar
end
```

---

## 3. Operadores

| Categoría | Operadores |
|---|---|
| Aritméticos | `+`  `-`  `*`  `/`  `%`  `//` (división entera)  `**` (potencia) |
| Comparación | `==`  `!=`  `<`  `>`  `<=`  `>=` |
| Lógicos (como palabra, no símbolo) | `and`  `or`  `not` |
| Asignación aumentada | `+=`  `-=`  `*=`  `/=`  `%=`  `//=` |
| Incremento/decremento | `++`  `--` |

Precedencia (de menor a mayor): `or` → `and` → `not` → comparación → `+ -` → `* / % //` →
unario (`-`, `not`, `++`, `--` prefijos) → `**` → postfijo (`.`, `[]`, `()`, `++`/`--` sufijos).

```ava
resultado = 2 + 3 * 4 ** 2   # ** liga más fuerte que *, que liga más fuerte que +
activo = not encontrado and intentos < 3
```

Los paréntesis agrupan una expresión para forzar precedencia (`groupAtom`):

```ava
resultado = (2 + 3) * 4
```

---

## 4. Control de flujo

### `if` / `elif` / `else`

```ava
if nota >= 90 then
    print("A")
elif nota >= 80 then
    print("B")
else
    print("F")
end
```

### `select` (switch al estilo VB6)

No lleva `end select` ni `case else`: cierra con un `end` solo, y el `else` final cubre el caso
por defecto. Cada `case` admite tres formas de comparar contra el valor de `select`:

```ava
func calificacion(nota)
    select nota
        case 90 to 100 then       # rango: nota >= 90 and nota <= 100
            return "A"
        case 80, 85, 89 then      # lista de valores exactos (OR)
            return "B"
        case is >= 60 then        # comparación relacional contra el valor
            return "C"
        else
            return "F"
    end
end
```

### `while`

```ava
while i < len(items)
    i++
end

while (i < 10)    # los paréntesis son opcionales
    i++
end
```

### `for ... in`

```ava
for item in items then
    print(item)
end

for item in (items) then       # el iterable puede ir entre paréntesis (opcional)
    print(item)
end

for clave, valor in pares then # multi-target: targetList admite más de un nombre
    print(clave)
end
```

### `break` / `continue` / `pass`

```ava
for x in items then
    if x == 0 then
        continue
    end
    if x < 0 then
        break
    end
    pass   # no-op, útil como placeholder de un bloque vacío
end
```

---

## 5. Funciones y lambdas

### Función con nombre

```ava
func add(a, b)
    return a + b
end
```

Parámetros con valor por defecto, tipo, y tipo de retorno (todo opcional y combinable):

```ava
func connect(host, port = 5432, timeout = 10)
    return host
end

func add(a as int, b as int) as int
    return a + b
end
```

Parámetro variádico (`*nombre`, recibe el resto de los argumentos):

```ava
func log(nivel, *mensajes)
    # ...
end
```

Llamada con argumentos posicionales o nombrados:

```ava
print(connect(host = "db.local", timeout = 5))
```

### `async func` / `await`

```ava
async func incrementar()
    await delay(100)
    return 1
end
```

`await expr` solo es válido dentro del cuerpo de una función declarada `async`; eso lo valida
el compilador, no la gramática (sintácticamente `await` es válido en cualquier expresión).

### Función con modificador (`static func` / `private func`)

Igual que los atributos, solo tiene sentido dentro de una clase:

```ava
class Contador
    static func reset()
        Contador.total = 0
    end

    private func log(msg)
        print(msg)
    end
end
```

### Lambdas

Tres formas, de más a menos azúcar sintáctico:

```ava
doble = x => x * 2                      # un solo parámetro, sin paréntesis, sin tipos
cuadrado = (x) => x ** 2                # con paréntesis (admite 0, 1 o más parámetros)
suma = (a, b) => a + b
tipada = (x as int) as int => x * 2     # con tipos: los paréntesis pasan a ser obligatorios

sumaLarga = func (a, b)                 # forma "func" con cuerpo de bloque completo
    return a + b
end
```

### `return`

```ava
return            # sin valor
return x
return x, y       # múltiples valores
```

---

## 6. Clases

```ava
class Animal
    static count = 0
    private _name = "unknown"

    func Animal(name)          # constructor: mismo nombre que la clase
        this._name = name
        Animal.count++
    end

    func speak()
        return this._name + " hace un sonido"
    end
end

class Dog: Animal               # `:` = herencia
    func speak()
        return base.speak() + "... pero más fuerte"   # `base` llama al método del padre
    end
end

perro = Dog("Rex")
print(perro.speak())
```

- `this` — referencia a la instancia actual (resaltado como variable especial en el editor).
- `base.metodo(...)` — llama a la implementación del método en la clase padre.
- Atributos con anotación de tipo dentro de la clase: `nombre as string`.

---

## 7. Manejo de errores

```ava
try
    raise "algo salió mal"
catch (err)
    print("capturado: " + err)
finally
    print("listo")
end
```

`catch` también admite la forma sin paréntesis (`catch err`). Se pueden encadenar varios
`catch`, y `finally` es opcional (pero si no hay ningún `catch`, tiene que haber `finally`).

---

## 8. Literales, strings y colecciones

### Números

```ava
10
3.14
```

⚠️ El punto decimal exige dígitos a **ambos** lados: `3.14` es válido, pero `.5` y `5.` no lo
son (no hay forma corta). Tampoco hay notación científica (`1e10`) ni literal negativo propio —
`-5` es el operador unario `-` aplicado a `5`, no un solo token.

### Strings

```ava
"comillas dobles"
'comillas simples'
"con escape: \n \t \" \\ \' \b \r"
```

Los únicos escapes válidos son `\b \t \n \r \" \' \\` — no hay `\xNN`, `\uNNNN` ni `\0`.

### F-strings (interpolación)

```ava
nombre = "Ava"
saludo = $"hola, {nombre}!"
anidado = $"resultado: {items[0] + items[1]}"     # las llaves admiten anidamiento
llaves_literales = $"esto es {{literal}}"          # {{ y }} escapan la llave
```

### Listas

```ava
items = [1, 2, 3]
primero = items[0]
vacia = []
```

Slices (`items[inicio:fin:paso]`, todos los campos opcionales):

```ava
items[1:2]      # del índice 1 al 2
items[:2]       # desde el principio hasta el índice 2
items[2:]       # desde el índice 2 hasta el final
items[:]        # copia completa
items[::2]      # con paso 2 (todo el rango, cada 2 elementos)
items[1:5:2]    # los tres campos juntos
```

### Diccionarios

```ava
puntajes = {alice: 10, bob: 20}          # claves sin comillas (estilo objeto)
config = {"timeout": 30, "modo": "prod"} # o con comillas (estilo JSON)
valor = puntajes["alice"]
```

### `true` / `false` / `nil`

```ava
activo = true
vacio = nil
```

---

## 9. Módulos, `extern` y funciones incorporadas

### `import`

```ava
import mysql
import mysql as db
import system            # habilita el namespace System.*
```

### `extern` (llamar código nativo — FFI)

Bloque de solo declaraciones (sin cuerpo), resuelto contra una librería nativa:

```ava
extern "kernel32" as K
    func Sleep(ms as int)
    func GetTickCount() as int
end

ahora as int = K.GetTickCount()
```

### Funciones incorporadas

> Esto **no sale de la gramática** (para el parser son llamadas comunes). Es lo que encontré
> realmente usado en los ejemplos del repo (`samples/`, `tools/vscode/examples/example.ava`):

| Función | Uso |
|---|---|
| `print(...)` | imprime en consola |
| `str(x)` | convierte a string |
| `len(x)` | longitud de una lista/string/dict |

### Módulo `System.*`

También ajeno a la gramática — API expuesta por el runtime, usada tras `import system`:

```ava
import system

System.Console.WriteLine($"hola desde {System.Environment.GetEnvironmentVariable("USER")}")
System.Console.ForegroundColor(System.Console.Colors.Cyan)
System.Console.ResetColor()

ahora = System.DateTime.Now()
System.DateTime.Sleep(10)

if System.IO.File.Exists("config.json") then
    contenido = System.IO.File.ReadAllText("config.json")
end

for entrada in System.IO.Directory.Enumerate(".") then
    print(entrada["Name"])
end

resultado = System.Diagnostics.Process.Start("echo", ["hi"])
```

Namespaces disponibles: `System.Console`, `System.DateTime`, `System.Environment`,
`System.IO.File`, `System.IO.Directory`, `System.Diagnostics.Process`.

---

## 10. Comentarios y fin de línea

```ava
# esto es un comentario de línea (no hay comentarios de bloque)
```

Cada statement termina con salto de línea (o `;`). No hace falta `;` al final de cada línea —
solo sirve para poner más de un statement simple en la misma línea.

---

## Referencia cruzada: palabra clave → dónde mirar

| Palabra clave | Sección |
|---|---|
| `if` `elif` `else` `then` `end` | [§4](#4-control-de-flujo) |
| `select` `case` `to` `is` | [§4](#4-control-de-flujo) |
| `while` `for` `in` | [§4](#4-control-de-flujo) |
| `break` `continue` `pass` | [§4](#4-control-de-flujo) |
| `func` `return` `async` `await` | [§5](#5-funciones-y-lambdas) |
| `class` `this` `base` `static` `private` | [§6](#6-clases) |
| `try` `catch` `finally` `raise` | [§7](#7-manejo-de-errores) |
| `true` `false` `nil` | [§8](#8-literales-strings-y-colecciones) |
| `import` `extern` `as` | [§9](#9-módulos-extern-y-funciones-incorporadas) |
| `local` `as` (anotación de tipo) | [§2](#2-variables-asignación-y-tipos) |
| `and` `or` `not` | [§3](#3-operadores) |
| `yield` | expresión (`yieldAtom` en `primary`, ver `AvaLang.g4`) — generadores, no cubierto todavía por ningún ejemplo del repo |

---

*Generado a partir de `runtime/avalang/grammar/AvaLang.g4`. Si la gramática cambia (nueva
palabra clave, nuevo operador, nueva forma de statement), este documento queda desactualizado —
volvé a compararlo contra el `.g4` antes de confiar en él para algo que no esté ya cubierto acá.*
