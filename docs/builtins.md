# Built-in Functions

Funciones disponibles globalmente en todo script AvaLang.

## print(...values)

Imprime valores a la consola.

```python
print("Hello")
print(1, 2, 3)
print("Name:", "Ava")
```

**Parámetros:** Cualquier número de valores
**Retorna:** `nil`

---

## type(value)

Retorna el tipo de un valor como string.

```python
type(42)           # "number"
type("hello")      # "string"
type(true)         # "bool"
type(nil)          # "nil"
type([1, 2])       # "list"
type({"a": 1})     # "dict"
```

**Parámetros:** 1 valor
**Retorna:** String con el tipo ("nil", "bool", "number", "string", "list", "dict", "function", "instance", "class", "native", "bound")

---

## str(value)

Convierte un valor a string.

```python
str(123)           # "123"
str(true)          # "true"
str(nil)          # "nil"
str([1, 2])        # "<object>" (listas no se convierten)
"Number: " + str(42)
```

**Parámetros:** 1 valor
**Retorna:** String representation

---

## int(value)

Convierte un valor a entero (trunca decimales).

```python
int(3.7)           # 3
int("42")          # 42
int(true)          # nil (no soportado)
```

**Parámetros:** 1 número o string numérico
**Retorna:** Entero o `nil` si falla

---

## float(value)

Convierte un valor a decimal.

```python
float(5)           # 5.0
float("3.14")      # 3.14
```

**Parámetros:** 1 número o string numérico
**Retorna:** Decimal o `nil` si falla

---

## abs(number)

Valor absoluto.

```python
abs(-5)            # 5
abs(5)             # 5
```

**Parámetros:** 1 número
**Retorna:** Valor absoluto

---

## round(number)

Redondea al entero más cercano.

```python
round(3.5)         # 4
round(3.4)         # 3
```

**Parámetros:** 1 número
**Retorna:** Entero redondeado

---

## floor(number)

Redondea hacia abajo.

```python
floor(3.9)         # 3
floor(3.1)         # 3
```

**Parámetros:** 1 número
**Retorna:** Entero más cercano hacia abajo

---

## ceil(number)

Redondea hacia arriba.

```python
ceil(3.1)          # 4
ceil(3.9)          # 4
```

**Parámetros:** 1 número
**Retorna:** Entero más cercano hacia arriba

---

## min(...numbers)

Retorna el mínimo de los argumentos.

```python
min(1, 5, 3)       # 1
min(10, 2, 8)      # 2
min(42)            # 42
```

**Parámetros:** 1 o más números
**Retorna:** El valor mínimo o `nil` si no hay números

---

## max(...numbers)

Retorna el máximo de los argumentos.

```python
max(1, 5, 3)       # 5
max(10, 2, 8)      # 10
```

**Parámetros:** 1 o más números
**Retorna:** El valor máximo o `nil` si no hay números

---

## pow(base, exponent)

Potencia (equivalente al operador `**`).

```python
pow(2, 3)          # 8
pow(4, 0.5)        # 2
```

**Parámetros:** base, exponent
**Retorna:** Resultado de base^exponent o `nil` si falla

---

## sqrt(number)

Raíz cuadrada.

```python
sqrt(16)           # 4
sqrt(81)           # 9
```

**Parámetros:** 1 número
**Retorna:** Raíz cuadrada o `nil` si no es número

---

## len(value)

Longitud de string, lista o dict.

```python
len("hello")       # 5
len([1, 2, 3])     # 3
len({"a": 1})      # 1
```

**Parámetros:** 1 string, lista o dict
**Retorna:** Longitud o `nil` si tipo no soportado

---

## range(start, end, step)

Genera lista de números.

```python
range(5)           # [0, 1, 2, 3, 4]
range(1, 5)        # [1, 2, 3, 4]
range(0, 10, 2)    # [0, 2, 4, 6, 8]
range(5, 0, -1)    # [5, 4, 3, 2, 1]
```

**Parámetros:** 
- end: número final (start=0, step=1)
- start, end: rango (step=1)
- start, end, step: rango completo

**Retorna:** Lista de números

---

## sum(list)

Suma elementos de una lista.

```python
sum([1, 2, 3, 4, 5])   # 15
sum([10, 20, 30])      # 60
```

**Parámetros:** 1 lista
**Retorna:** Suma de elementos numéricos o `nil`

---

## sorted(list)

Retorna lista ordenada (ascendente).

```python
sorted([5, 3, 1, 4, 2])    # [1, 2, 3, 4, 5]
sorted([10, -5, 3, 8])     # [-5, 3, 8, 10]
```

**Parámetros:** 1 lista
**Retorna:** Nueva lista ordenada

---

## reversed(list)

Retorna lista invertida.

```python
reversed([1, 2, 3, 4])     # [4, 3, 2, 1]
reversed(["a", "b", "c"])  # ["c", "b", "a"]
```

**Parámetros:** 1 lista
**Retorna:** Nueva lista en orden inverso

---

## any(list)

Retorna `true` si algún elemento es truthy.

```python
any([true, false, false])   # true
any([false, false])        # false
any([0, 1, 2])             # true (1 es truthy)
```

**Parámetros:** 1 lista
**Retorna:** `true` o `false`

---

## all(list)

Retorna `true` si todos los elementos son truthy.

```python
all([true, true, true])    # true
all([true, false])         # false
all([1, 2, 3])             # true (todos non-zero)
```

**Parámetros:** 1 lista
**Retorna:** `true` o `false`

---

## Resumen Rápido

| Función | Descripción |
|---------|-------------|
| `print(...)` | Imprime valores |
| `type(x)` | Tipo como string |
| `str(x)` | Convierte a string |
| `int(x)` | Convierte a entero |
| `float(x)` | Convierte a decimal |
| `abs(x)` | Valor absoluto |
| `round(x)` | Redondea |
| `floor(x)` | Redondea hacia abajo |
| `ceil(x)` | Redondea hacia arriba |
| `min(a, b, ...)` | Mínimo |
| `max(a, b, ...)` | Máximo |
| `pow(x, y)` | Potencia |
| `sqrt(x)` | Raíz cuadrada |
| `len(x)` | Longitud |
| `range(start, end, step)` | Secuencia |
| `sum(list)` | Suma elementos |
| `sorted(list)` | Lista ordenada |
| `reversed(list)` | Lista invertida |
| `any(list)` | Algún elemento truthy |
| `all(list)` | Todos elementos truthy |