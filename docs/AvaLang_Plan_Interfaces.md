# Plan — Interfaces para AvaLang (diseño y fases)

## Propósito

Agregar `interface` a AvaLang con el mismo espíritu que `class`: sintaxis
estilo C#, resuelto en su mayoría en tiempo de compilación (como ya hace
`override`), con el mínimo de estado nuevo en la VM. Alcance acordado:

- Firmas de método **y** métodos con cuerpo por defecto (default interface
  methods, estilo C# 8+).
- Una clase puede implementar **varias** interfaces a la vez, además de
  heredar de una base class.
- Soporte runtime real: operador `is` y builtin `typeof`, no solo
  validación en compile-time.

## 1. Sintaxis

### 1.1 Declaración de interfaz

```
interface IShape
    func Area() as float
    func Perimeter() as float

    // método con cuerpo = implementación por defecto
    func Describe() as string
        return "shape, area=" + this.Area()
    end
end
```

Reutiliza el mismo molde que `externFuncDeclaration` para las firmas sin
cuerpo (`func NAME(params) returnType?`, sin `block`/`end` propio) y
`funcDeclaration` tal cual para los métodos con cuerpo — nada nuevo del
lado de parámetros/tipos.

### 1.2 Herencia de interfaces (interface a interface)

```
interface IMovable : IShape
    func Move(dx as float, dy as float)
end
```

Igual que con `class`, una interfaz solo puede extender interfaces ya
compiladas más arriba en el archivo.

### 1.3 `classHeritage` extendido (multi-implementación)

```
class Circle : Shape, IShape, IMovable
    ...
end
```

`classHeritage` pasa de `':' NAME` a `':' NAME (',' NAME)*`. Regla de
resolución (igual que C#): como máximo un elemento de la lista puede ser
una **clase** (base class), y si está presente debe ser el primero; el
resto deben resolver contra interfaces conocidas. El compilador decide
qué es cada NAME buscándolo en `compiled_classes_` vs `compiled_interfaces_`
— no hace falta un keyword `implements` separado.

### 1.4 Operador `is` (runtime)

```
if shape is IShape then
    print(shape.Describe())
end
```

Nueva alternativa en `notExpr`/`comparison` (no hay conflicto con el `is`
que ya existe en `caseItemRelational` — ese vive en una posición distinta
de la gramática, como primer token de la alternativa `'is' compOp expr`
dentro de `caseItem`, nunca como sufijo de una expresión normal):

```
notExpr
    : 'not' notExpr
    | comparison ('is' NAME)?
    ;
```

Funciona tanto para clases (`obj is Circle`) como para interfaces
(`obj is IShape`), incluyendo la cadena de herencia/implementación
completa.

### 1.5 `typeof` (builtin, no keyword nuevo)

```
n = typeof(shape)   // "Circle" -- nombre de la clase concreta en runtime
```

Se agrega como builtin (mismo mecanismo que otros builtins en
`builtin_registry.cpp`), no como palabra reservada.

## 2. Semántica

### 2.1 Métodos abstractos vs. default

- Firma sin cuerpo (`func Area() as float`) → **obligatorio**: toda clase
  que implemente la interfaz (directa o vía su cadena de bases) debe
  definir un método con ese nombre. Se valida en `CompileClass`, mismo
  lugar y mismo estilo de error que la validación de `override`.
- Firma con cuerpo (`func Describe() ... end`) → **default method**: si
  la clase (ni su cadena de bases) no define un método con ese nombre, se
  copia el `Proto` compilado de la interfaz a `class_obj->methods`, igual
  que ya se hace hoy con los métodos heredados de `base_class`.

### 2.2 Conflicto de default methods (diamond)

Si dos interfaces implementadas por la misma clase aportan un default
method con el mismo nombre y la clase no lo sobreescribe explícitamente,
es error de compilación: *"class 'X' inherits conflicting default
implementations of 'Y' from 'IA' and 'IB' -- provide an explicit
override"*. Igual que C#, no hay resolución automática.

### 2.3 Validación en compile-time

Mismo checklist que ya corre para `base_class` en `CompileClass`, mismo
mensaje de error en el mismo `AvaError` con línea/columna, y misma
comprobación con `class_method_params_`/`class_method_returns_` para que
`CheckMethodCallArgs` valide llamadas a través de una variable tipada
`as IShape`.

### 2.4 Herencia de interfaces vía `import`

Se apoya en el `HarvestedClassInfo`/`SnapshotClassInfo` que ya existe
para clases importadas (ver bug de imports que ya arreglaste) — se
agrega el equivalente para interfaces al mismo mecanismo, para que una
interfaz definida en otro archivo `.ava` funcione igual de bien
implementada/importada que una clase.

## 3. Modelo interno (qué archivo toca qué)

| Pieza | Archivo | Qué agrega |
|---|---|---|
| Gramática | `grammar/AvaLang.g4` | `interfaceDeclaration`, `interfaceHeritage`, `interfaceMethodSignature` (firma sin cuerpo, agregada a `smallStatement` igual que `memberModifier` -- válida sintácticamente donde sea, restringida por el compilador a cuerpo de interfaz), `classHeritage` con lista, `'is' NAME` en `notExpr` |
| AST | `ast/ast.h`, `ast/ast_builder.cpp` | `InterfaceDef` (nombre, interfaces base, lista de firmas + métodos default), `ClassDef::interfaces` (lista de NAME, adicional a `base_class`), `IsExpr` (expr binaria: operando + nombre de tipo) |
| Compilador | `compiler/compiler.h`, `compiler/compiler.cpp` | `compiled_interfaces_` (mapa nombre -> InterfaceObj*), `interface_method_signatures_`/`interface_default_methods_`, `CompileInterface(const InterfaceDef*)`, extender `CompileClass` (resolución de `classHeritage` multi, validación de métodos faltantes, copia de default methods, detección de diamond), `CompileExpr` caso `IsExpr` -> nuevo opcode |
| VM (runtime) | `vm/value.h` | `ClassObj::implemented_interfaces` (`unordered_set<string>`, transitiva) |
| VM (runtime) | `vm/opcodes.h`, `vm/vm_classes.cpp` (o `vm_compare.cpp`) | opcode `OP_IS` (compara nombre de clase concreta + `implemented_interfaces` contra el NAME constante) |
| Builtins | `builtins/builtin_registry.cpp`, `builtin_names.h` | `typeof(obj)` |

## 4. Fases de implementación

| Fase | Contenido | Entregable |
|---|---|---|
| 1 | Gramática + AST: `interfaceDeclaration`, `interfaceHeritage`, `interfaceMethodSignature`, `classHeritage` multi, `IsExpr`. Sin codegen todavía (falla con "not implemented" si se usa) | zip |
| 2 | Compilador: `compiled_interfaces_`, `CompileInterface`, resolución de `classHeritage` multi (clase vs. interfaz), copia de default methods, validación de métodos faltantes | zip |
| 3 | Compilador: detección de conflicto diamond, extensión de interfaz a interfaz, soporte vía `import` (armonizar con `HarvestedClassInfo`) | zip |
| 4 | VM: `ClassObj::implemented_interfaces`, opcode `OP_IS`, codegen de `IsExpr` | zip |
| 5 | Builtin `typeof` + pulido de mensajes de error + wiki (`docs/wiki/`) si querés documentarlo igual que Button/TextBox | zip |

¿Alguna fase la querés partir más chica, o arrancamos con la Fase 1 tal
como está?
