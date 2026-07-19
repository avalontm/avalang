# Opcodes y Bytecode

Documentación técnica del conjunto de instrucciones y formato de bytecode del VM register-based de AvaLang.

## Formato de Instrucción

Las instrucciones usan formato iABC con operandos de ancho fijo:

```
struct Instr {
    OpCode  op;     // Código de operación (1 byte)
    uint8_t a;      // Registro destino o primer operando
    uint16_t b;     // Segundo operando (también Bx para operandos extendidos)
    uint16_t c;     // Tercer operando (también sBx para saltos relativos)
};
```

### Convenciones de operandos

| Formato | Descripción |
|---------|-------------|
| `A` | Registro destino |
| `B`, `C` | Registros fuente |
| `Bx` | Índice en tabla de constantes (16 bits) |
| `sBx` | Offset de salto con signo (16 bits) |

---

## Opcodes Completos

### Carga de Valores

| Opcode | A | B/C | Descripción |
|--------|---|-----|-------------|
| `LOADK` | dst | Bx | `R[A] = K[Bx]` - Carga constante del pool |
| `LOADNIL` | dst | - | `R[A] = nil` |
| `LOADBOOL` | dst | B | `R[A] = (bool)B` - Carga valor booleano inmediato |
| `MOVE` | dst | src | `R[A] = R[B]` - Copia registro a registro |

```python
# LOADK - carga constante
x = 42         # LOADK R0, K[0]   ; 42
y = "hello"    # LOADK R1, K[1]   ; "hello"

# LOADNIL - valor nulo
x = nil        # LOADNIL R0

# LOADBOOL - booleano
x = true       # LOADBOOL R0, 1
x = false      # LOADBOOL R0, 0

# MOVE - copiar registro
y = x          # MOVE R1, R0
```

### Acceso a Variables

| Opcode | A | B/C | Descripción |
|--------|---|-----|-------------|
| `GETGLOBAL` | dst | Bx | `R[A] = Globals[K[Bx]]` |
| `SETGLOBAL` | src | Bx | `Globals[K[Bx]] = R[A]` |
| `GETUPVAL` | dst | B | `R[A] = Upval[B]` |
| `SETUPVAL` | src | B | `Upval[B] = R[A]` |

```python
# GETGLOBAL - leer variable global
print(x)        # GETGLOBAL R1, K["x"]
                # CALL print, 1, 0

# SETGLOBAL - escribir variable global
x = 10          # LOADK R0, K[10]
                # SETGLOBAL R0, K["x"]
```

### Listas y Diccionarios

| Opcode | A | B/C | Descripción |
|--------|---|-----|-------------|
| `NEWLIST` | dst | - | `R[A] = new List` - Crea lista vacía |
| `NEWDICT` | dst | - | `R[A] = new Dict` - Crea diccionario vacío |
| `LISTAPPEND` | list | item | `R[A].append(R[B])` |

```python
# NEWLIST + LISTAPPEND
nums = [1, 2, 3]
# NEWLIST R0
# LOADK R1, K[1]
# LISTAPPEND R0, R1
# LOADK R2, K[2]
# LISTAPPEND R0, R2
# ...

# NEWDICT
data = {}
# NEWDICT R0
```

### Indexación

| Opcode | A | B | C | Descripción |
|--------|---|---|---|-------------|
| `GETINDEX` | dst | obj | idx | `R[A] = R[B][R[C]]` |
| `SETINDEX` | obj | idx | val | `R[A][R[B]] = R[C]` |

```python
# GETINDEX
x = nums[1]     # GETINDEX R0, R[nums], R[1]

# SETINDEX
nums[0] = 10    # LOADK R2, K[10]
                # SETINDEX R[nums], R[0], R[2]
```

### Atributos de Objeto

| Opcode | A | B | Bx/C | Descripción |
|--------|---|---|------|-------------|
| `GETATTR` | dst | obj | Bx | `R[A] = R[B].K[Bx]` |
| `SETATTR` | obj | Bx | C | `R[A].K[Bx] = R[C]` |

```python
# GETATTR - acceder atributo
print(p.x)     # GETATTR R1, R[p], K["x"]
                # CALL print, 1, 0

# SETATTR - asignar atributo
p.x = 10       # LOADK R2, K[10]
                # SETATTR R[p], K["x"], R[2]
```

### Operadores Aritméticos

| Opcode | A | B | C | Descripción |
|--------|---|---|---|-------------|
| `ADD` | dst | a | b | `R[A] = R[B] + R[C]` |
| `SUB` | dst | a | b | `R[A] = R[B] - R[C]` |
| `MUL` | dst | a | b | `R[A] = R[B] * R[C]` |
| `DIV` | dst | a | b | `R[A] = R[B] / R[C]` |
| `MOD` | dst | a | b | `R[A] = R[B] % R[C]` |
| `POW` | dst | a | b | `R[A] = R[B] ** R[C]` |
| `NEG` | dst | src | - | `R[A] = -R[B]` |

```python
# Binarios
z = x + y       # ADD R2, R[x], R[y]
z = x - y       # SUB R2, R[x], R[y]
z = x * y       # MUL R2, R[x], R[y]
z = x / y       # DIV R2, R[x], R[y]

# Unario
z = -x          # NEG R1, R[x]
```

### Operadores de Comparación

| Opcode | A | B | C | Descripción |
|--------|---|---|---|-------------|
| `EQ` | dst | a | b | `R[A] = R[B] == R[C]` |
| `NE` | dst | a | b | `R[A] = R[B] != R[C]` |
| `LT` | dst | a | b | `R[A] = R[B] < R[C]` |
| `LE` | dst | a | b | `R[A] = R[B] <= R[C]` |
| `GT` | dst | a | b | `R[A] = R[B] > R[C]` |
| `GE` | dst | a | b | `R[A] = R[B] >= R[C]` |

```python
# Comparaciones
if x == y      # EQ R0, R[x], R[y]
if x < y       # LT R0, R[x], R[y]
if x >= 0      # GE R0, R[x], K[0]
```

### Operador Unario Lógico

| Opcode | A | B | C | Descripción |
|--------|---|---|---|-------------|
| `NOT` | dst | src | - | `R[A] = !truthy(R[B])` |

```python
# Negación lógica
if not x       # NOT R0, R[x]
```

### Control de Flujo - Saltos

| Opcode | A | Bx/sBx | Descripción |
|--------|---|--------|-------------|
| `JMP` | - | sBx | `pc += sBx` - Salto relativo |
| `TEST` | reg | C | `if truthy(R[A]) != C then pc++` |

```python
# JMP - salto incondicional
# Usado para if/else/while/for

# TEST - salto condicional
if x           # TEST R[x], 1
                # JMP else_label
```

### Llamadas a Función

| Opcode | A | B | C | Descripción |
|--------|---|---|---|-------------|
| `CALL` | callee | argc | retc | `R[A..] = call R[A](R[A+1..])` |
| `RETURN` | val | count | - | `return R[A..A+B-1]` |

```python
# Llamada simple
greet("world") # LOADK R1, K["world"]
                # CALL R[greet], 1, 1

# Return
return x       # MOVE R0, R[x]
                # RETURN R0, 1
```

### Closures y Clases

| Opcode | A | Bx/B | Descripción |
|--------|---|------|-------------|
| `CLOSURE` | dst | Bx | `R[A] = make closure from child_protos[Bx]` |
| `NEWCLASS` | dst | Bx | `R[A] = new Class from K[Bx]` |
| `NEWINSTANCE` | dst | B | `R[A] = new Instance of class R[B]` |
| `BASECALL` | A | Bx | C | Llama método de clase base |

```python
# CLOSURE - crear función
func inner()    # Closure compilado como child_proto[0]
    ...
end
# CLOSURE R0, 0    ; crea closure de child_proto[0]
# SETGLOBAL R0, K["inner"]

# NEWCLASS - crear clase
# Compilación de clase genera ClassObj
# NEWCLASS R0, K[class_template]

# NEWINSTANCE - instanciar
p = Point(1, 2) # NEWINSTANCE R0, R[Point]
                # CALL R0, 2, 0
```

### Coroutines (No implementado)

| Opcode | A | B | Descripción |
|--------|---|---|-------------|
| `YIELD` | val | count | `yield R[A..A+B-1]` |
| `RESUME` | dst | coroutine | C args | `resume R[B] with args` |

---

## Tabla de Constantes (K)

Cada Proto tiene un pool de constantes accedido por índice (Bx):

```
Proto {
    constants: Value[]   // Pool de constantes
}
```

### Tipos de constantes:
- Números: `Value::Number(42.0)`
- Strings: `Value::String(new StringObj("name"))`
- Clases: `Value::Class(new ClassObj())`

---

## Estructura de Proto

```cpp
struct Proto {
    std::vector<Instr> instructions;    // Bytecode
    std::vector<Value> constants;       // Pool de constantes
    std::vector<std::shared_ptr<Proto>> child_protos;  // Funciones anidadas
    uint8_t num_registers;              // Registers usados
    uint8_t num_params;                 // Parámetros de función
    bool is_vararg;                     // Es variádica?
    bool is_method;                     // Es método?
    std::string debug_name;             // Nombre para debug
};
```

---

## CallFrame

```cpp
struct CallFrame {
    std::shared_ptr<Proto> proto;
    std::shared_ptr<Closure> closure;  // Para closures
    std::vector<Value> registers;
    uint32_t pc = 0;
};
```

---

## Ejemplo de Descompilación

Código AvaLang:
```python
func add(a, b)
    return a + b
end
```

Bytecode generado:
```
; Function add (2 params, 4 registers)
LOADK R0, K[0]          ; carga "a"
LOADK R1, K[1]          ; carga "b"
ADD R2, R0, R1          ; R2 = R0 + R1
MOVE R0, R2              ; R0 = R2
RETURN R0, 1             ; return R0
```

---

## Registro de Opcodes por Estado

| Opcode | Estado | Notas |
|--------|--------|-------|
| LOADK, LOADNIL, LOADBOOL, MOVE | ✅ Implementado | |
| GETGLOBAL, SETGLOBAL | ✅ Implementado | |
| GETUPVAL, SETUPVAL | ⚪ Reservado | No usado aún |
| GETINDEX, SETINDEX | ✅ Implementado | |
| GETATTR, SETATTR | ✅ Implementado | |
| ADD, SUB, MUL, DIV, MOD, POW | ✅ Implementado | |
| NEG, NOT | ✅ Implementado | |
| EQ, NE, LT, LE, GT, GE | ✅ Implementado | |
| JMP, TEST | ✅ Implementado | |
| CALL, RETURN | ✅ Implementado | |
| CLOSURE | ✅ Implementado | |
| NEWCLASS, NEWINSTANCE | ✅ Implementado | |
| BASECALL | ✅ Implementado | |
| NEWLIST, LISTAPPEND, NEWDICT | ✅ Implementado | |
| YIELD, RESUME | ❌ No implementado | |