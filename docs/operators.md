# Operadores

## Aritméticos

```python
a = 10
b = 3

print(a + b)   # 13  (suma)
print(a - b)   # 7   (resta)
print(a * b)   # 30  (multiplicación)
print(a / b)   # 3.333... (división)
print(a % b)   # 1   (módulo)
print(a ** b)  # 1000 (potencia)
print(-a)      # -10 (negación)
```

---

## Comparación

```python
x = 5
y = 10

print(x == y)  # false
print(x != y)  # true
print(x < y)   # true
print(x <= y)  # true
print(x > y)   # false
print(x >= y)  # false

# Comparaciones de strings
print("a" < "b")         # true
print("apple" < "banana") # true
print("hello" == "hello") # true
print("hello" != "world")  # true
```

---

## Lógicos

```python
a = true
b = false

print(a and b)  # false
print(a or b)   # true
print(not a)    # false
```

### Tabla de verdad

| a | b | a and b | a or b |
|---|-----|---------|--------|
| true | true | true | true |
| true | false | false | true |
| false | true | false | true |
| false | false | false | false |

---

## Concatenación

```python
# Strings
"Hello" + " " + "World"  # "Hello World"

# Listas
[1, 2] + [3, 4]           # [1, 2, 3, 4]

# String + número (conversión automática)
"Count: " + 42            # "Count: 42"
```

---

## Precedencia de operadores

De mayor a menor prioridad:

1. `**` (potencia)
2. `*`, `/`, `%` (multiplicación, división, módulo)
3. `+`, `-` (suma, resta)
4. `<`, `<=`, `>`, `>=` (comparación)
5. `==`, `!=` (igualdad)
6. `not` (lógico NOT)
7. `and` (lógico AND)
8. `or` (lógico OR)

```python
# Ejemplo
2 + 3 * 4       # 14 (no 20)
(2 + 3) * 4     # 20

2 + 3 == 5      # true
2 + 3 == 5 and 1 > 0  # true
```

---

## Asignación compuesta

```python
x = 10
x = x + 5       # 15
x = x - 3       # 12
x = x * 2       # 24
```

---

## Operador ternario

No soportado. Usar if/else:

```python
# No soportado:
# result = x > 0 ? "positive" : "non-positive"

# Usar:
if x > 0 then
    result = "positive"
else
    result = "non-positive"
end
```

---

## Resumen

| Categoría | Operadores |
|-----------|------------|
| Aritméticos | `+`, `-`, `*`, `/`, `%`, `**`, `-` (unario) |
| Comparación | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Lógicos | `and`, `or`, `not` |
| Concatenación | `+` (strings, listas) |