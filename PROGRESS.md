# AvaLang - Estado del Desarrollo

## Resumen

El proyecto AvaLang es un lenguaje de scripting embebible con sintaxis tipo Python y semántica tipo Lua. Implementación completa de un bytecode VM register-based con soporte para clases, objetos, funciones, closures, y estructuras de control.

## Arquitectura Implementada

### VM Register-Based
- Acceso a frames por índice (evita dangling references)
- Opcodes aritméticos, lógicos y de control
- Llamadas de función con manejo correcto de argumentos
- Bound methods para métodos de instancia
- Soporte para herencia con `base()` / `super()`

### Value Model
- Tag-union: Nil, Bool, Number, String, List, Dict, Function, Native, Instance, Class, Coroutine, Bound
- Reference counting para garbage collection
- Conversión a/from C API (ava_value_t)

### Compilador
- AST → bytecode (registros virtuales)
- Compilación de clases con methods, attrs, y __init__
- Expresiones: BinOp, UnOp, Call, Index, Attr, List, Dict
- **F-String expressions** con interpolación en tiempo de compilación
- Control flow: if/elif/else, while, for, return, break, continue

## Funcionalidades Implementadas

### Built-in Functions
```python
# Conversión
type(x)              # "nil", "bool", "number", "string", "list", "dict", "function", "instance", "class"
str(x)              # Convierte a string
int(x)              # Convierte a entero (trunca decimales)
float(x)            # Convierte a decimal

# Matemáticas
abs(x)              # Valor absoluto
round(x)            # Redondea al entero más cercano
floor(x)            # Redondea hacia abajo
ceil(x)             # Redondea hacia arriba
min(a, b, ...)      # Mínimo de argumentos
max(a, b, ...)      # Máximo de argumentos
pow(x, y)           # Potencia (equivalente a x ** y)
sqrt(x)             # Raíz cuadrada

# Listas
sum(list)           # Suma elementos
sorted(list)        # Lista ordenada
reversed(list)      # Lista invertida
any(list)           # True si algún elemento es truthy
all(list)           # True si todos los elementos son truthy
len(x)              # Longitud de string/lista/dict
range(start, end, step)  # Generador de secuencia

# Ejemplos
print(type([1, 2, 3]))     # list
print(str(42))            # 42
print(int(3.7))           # 3
print(abs(-5))            # 5
print(min(3, 1, 4, 1))    # 1
print(sorted([3, 1, 4]))   # [1, 3, 4]
print(sum([1, 2, 3]))     # 6
```

### Tipos de Datos
```python
# Números
x = 42
y = 3.14
print(x + y)

# Strings
name = "Ava"
greeting = "Hello " + name

# Booleanos
flag = true
if flag: print("yes")

# Listas
nums = [1, 2, 3]
print(nums[1])  # 2
nums[0] = 10

# Diccionarios
data = {"name": "Ava", "age": 1}
data["version"] = "1.0"
```

### Clases y Objetos
```python
class Point(x, y)
    __init__(self, x, y)
        self.x = x
        self.y = y
    end
    
    show(self)
        print(self.x)
        print(self.y)
    end
end

p = Point(3, 4)
p.show()
```

### Herencia con base()
```python
class Animal(name)
    __init__(self, name)
        self.name = name
    end
    
    speak(self)
        print("...")
    end
end

class Dog(name)
    __init__(self, name)
        base(name)
    end
    
    speak(self)
        print(self.name + " says Woof!")
    end
end

d = Dog("Buddy")
d.speak()  # Buddy says Woof!
```

### Funciones
```python
func greet(name)
    print("Hello " + name)
end
greet("World")

func add(a, b)
    return a + b
end
print(add(3, 4))

func outer()
    x = 10
    func inner()
        print(x)
    end
    inner()
end
outer()
```

### Control Flow
```python
# If/elif/else
x = 1
if (x == 1) then
    print("one")
elif (x == 2) then
    print("two")
else
    print("other")
end

# While loop
i = 0
while (i < 3) do
    print(i)
    i = i + 1
end

# For loop
for item in [1, 2, 3] do
    print(item)
end

# Break y Continue
j = 0
while true do
    j = j + 1
    if (j == 5) then
        break
    end
    if (j == 2) then
        continue
    end
    print(j)
end
```

### Operadores
```python
# Aritméticos: +, -, *, /, %, **
# Comparación: ==, !=, <, <=, >, >=
# Lógicos: and, or, not
# Acceso: obj.attr, list[index], dict["key"]
```

### F-Strings
```python
# Sintaxis: $"..." con {expresion} para interpolar
name = "Ava"
age = 25

print($"Hello {name}!")           # Hello Ava!
print($"Age: {age}")               # Age: 25
print($"Sum: {3 + 4}")             # Sum: 7
print($"Braces: {{literal}}")      # Braces: {literal}

# Expresiones complejas
list = [1, 2, 3]
print($"First: {list[0]}")         # First: 1

class User(name)
    greet(self)
        return $"Hola {this.name}"
    end
end
u = User("Ana")
print($"Saludo: {u.greet()}")      # Saludo: Hola Ana
```

## Opcodes Implementados

| Opcode | Descripción | Estado |
|--------|-------------|--------|
| LOADK, LOADNIL, LOADBOOL, MOVE | Carga de valores | ✅ |
| GETGLOBAL, SETGLOBAL | Variables globales | ✅ |
| GETINDEX, SETINDEX | Indexación lista/dict | ✅ |
| GETATTR, SETATTR | Atributos de objeto | ✅ |
| ADD, SUB, MUL, DIV, MOD, POW | Aritmética | ✅ |
| NEG, NOT | Unarios | ✅ |
| EQ, NE, LT, LE, GT, GE | Comparación | ✅ |
| JMP, TEST | Saltos | ✅ |
| CALL, RETURN | Llamadas a función | ✅ |
| CLOSURE | Creación de closures | ✅ |
| NEWLIST, LISTAPPEND | Listas | ✅ |
| NEWDICT | Diccionarios | ✅ |
| NEWCLASS, NEWINSTANCE | Clases e instancias | ✅ |
| BASECALL | Llamadas a método base | ✅ |
| GETUPVAL, SETUPVAL | Upvalues | ❌ No usado |
| YIELD, RESUME | Coroutines | ❌ |
| FOR, FORPREP | Iteración | ❌ (implementado en compiler) |

## Estructura del Proyecto

```
avalang/
├── include/ava.h           # API pública C
├── grammar/AvaLang.g4     # Gramática ANTLR4
├── src/
│   ├── ast/
│   │   ├── ast.h          # Nodos AST
│   │   └── ast_builder.cpp # Parser → AST
│   ├── compiler/
│   │   ├── compiler.cpp   # AST → bytecode
│   │   └── compiler.h
│   ├── frontend/
│   │   ├── frontend.h     # Interfaz
│   │   ├── frontend_stub.cpp
│   │   └── frontend_antlr.cpp
│   ├── vm/
│   │   ├── vm.cpp         # Ejecutor bytecode
│   │   ├── vm.h
│   │   ├── value.h/cpp   # Value, Objects
│   │   ├── opcodes.h      # Definición opcodes
│   │   ├── proto.h        # Proto/Closure
│   │   ├── closure.h
│   │   └── coroutine.h/cpp
│   ├── api/
│   │   └── c_api.cpp      # Implementación C API
│   └── cli/
│       └── main.cpp       # ava_cli
├── scripts/                 # Scripts de prueba .ava
└── progress.md             # Este archivo
```

## Testing Scripts

Scripts de prueba disponibles en `scripts/`:

```bash
# Windows
build\Release\ava_cli.exe scripts\<test>.ava

# Linux/macOS
./build/ava_cli scripts/<test>.ava
```

### Scripts de prueba:

| Script | Descripción |
|--------|-------------|
| `test_arithmetic.ava` | Operaciones aritméticas (+, -, *, /, %, **) |
| `test_variables.ava` | Variables y booleanos |
| `test_control_flow.ava` | if/while/break/continue |
| `test_lists_dicts.ava` | Listas y diccionarios |
| `test_functions.ava` | Funciones, closures y recursión |
| `test_parens.ava` | Paréntesis opcionales |
| `test_builtins.ava` | Todas las funciones built-in |
| `test_class_inherit.ava` | Herencia de clases |
| `test_class_multilevel.ava` | Herencia multinivel |
| `test_base_simple.ava` | base() con argumentos |
| `test_super_simple.ava` | base() sin argumentos |
| `test_debug_base.ava` | Debug de herencia |
| `test_this2.ava` | this implícito en métodos |
| `test_import.ava` | Import con alias |
| `test_fstrings.ava` | F-strings con interpolación |
| `test_operators_bugs.ava` | Bugs de operadores (comparaciones, AugAssign, self) |

## Comandos de Build

```bash
# Windows
build.bat              # Release (lib + ava_cli.exe)
build.bat debug        # Debug build
build.bat ninja        # Use Ninja generator

# Linux/macOS
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Manejo de Errores de Sintaxis

El compilador ahora proporciona mensajes de error detallados con:
- **Línea y columna exactas** del error
- **Mensaje descriptivo** indicando qué se esperaba y qué se encontró
- **Visualización del código** con caret debajo del token ofensivo

### Formato de Error
```
error at script.ava:5:12: missing 'then' at '\n    '
    5 | if x > 0
              ^
```

### Ejemplo de errores capturados
```bash
# Falta 'then' en if
error at test.ava:3:9: missing 'then' at '\n    '
    3 | if x > 0
              ^

# Paréntesis sin cerrar (múltiples errores)
error at test.ava:3:15: missing ')' at '\n'
    3 |     x = (1 + 2
                    ^
error at test.ava:4:1: missing 'end' at '<EOF>'

# Token inesperado
error at test.ava:2:8: extraneous input '+' expecting {...}
    2 | x = 1 ++ 2
             ^
```

## Estado Actual

### ✅ Funcional
- Variables y expresiones
- Aritmética y comparaciones
- **Mensajes de error detallados con línea, columna y visualización del código**
- Funciones con parámetros y return
- Closures (funciones anidadas)
- Clases con métodos y atributos
- Herencia de atributos en __init__
- **Herencia completa con `base()` y `super()`**
- Listas y diccionarios
- If/elif/else, while loops
- **For loops con `for var in iterable`**
- Break y continue
- **Módulos/import con alias (`import "module" as m`)**
- **String methods** (`.upper()`, `.lower()`, `.split()`, `.trim()`, `.contains()`, `.replace()`, `.indexOf()`, `.startsWith()`, `.endsWith()`, `.substring()`, `.length`)
- **List methods** (`.append()`, `.push()`, `.pop()`, `.insert()`, `.remove()`, `.length`, `.contains()`)
- **Dict methods** (`.keys()`, `.values()`, `.items()`, `.length`, `.containsKey()`)
- **F-strings** con sintaxis `$"..."` para interpolación de expresiones

### ⚠️ Limitaciones Conocidas
- No hay soporte para exceptions
- No hay soporte para decorators
- No hay soporte para list/dict comprehensions
- Coroutines no implementadas (YIELD/RESUME)

### ❌ Por Implementar
- `try`/`except` (excepciones)
- Generators (yield en funciones)
- Decorators

---

**Fecha:** 2026-07-19
**Estado:** Desarrollo activo - funcionalidades core completas
**Highlights:**
- **Comparaciones de strings corregidas**: EQ, NE, LT, LE, GT, GE ahora funcionan correctamente con strings
- **AugAssign con atributos**: `self.attr += 1` ahora funciona (antes retornaba nil)
- **Keyword `self`**: Ahora es reconocido como alias de `this` en el compilador
- Documentación completa de Opcodes y Bytecode en `docs/opcodes.md`
- Módulos/imports implementados con ModuleResolver y ModuleCache
- Sistema de cache de módulos para imports circulares
- Support para import con alias: `import "module" as m`
- Herencia de clases con `base()` corregida y funcional
- **String methods** documentados en `docs/string-methods.md`
- **List methods** documentados en `docs/list-methods.md`
- **Dict methods** documentados en `docs/dict-methods.md`
- **F-strings** implementados con sintaxis `$"..."` para interpolación de expresiones
- **F-strings** documentados en `docs/fstrings.md`

### Estructura de Opcodes
- 34 opcodes documentados (incluyendo YIELD/RESUME no implementados)
- Formato iABC con operandos de ancho fijo
- Tabla de constantes (K) para números, strings, clases
- CallFrames con registers virtuales para frames de función