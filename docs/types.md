# Tipos de Datos

## nil

Valor nulo/ausente.

```python
x = nil
print(x)  # nil
```

---

## Booleanos

```python
a = true
b = false

if a then
    print("a is true")
end
```

Valores **falsy**: `nil`, `false`
Valores **truthy**: todo lo demás (números distintos de 0, strings no vacíos, listas, etc.)

---

## Números

Todos los números son de punto flotante de doble precisión.

```python
x = 42
y = 3.14
z = -7

print(x + y)   # 45.14
print(x * 2)   # 84
print(y / 2)   # 1.57
```

---

## Strings

```python
name = "Ava"
greeting = "Hello, " + name + "!"
print(greeting)  # Hello, Ava!
```

### Concatenación

```python
"Hello" + " " + "World"  # "Hello World"
"Number: " + str(42)     # "Number: 42"
```

---

## Listas

Colección ordenada de valores.

```python
nums = [1, 2, 3, 4, 5]
mixed = ["hello", 42, true]

print(nums[0])  # 1
print(nums[2])  # 3

nums[0] = 10
print(nums[0])  # 10
```

### Operaciones con listas

```python
# Concatenar
[1, 2] + [3, 4]    # [1, 2, 3, 4]

# Indexar
[1, 2, 3][1]       # 2

# Longitud
len([1, 2, 3])     # 3

# Pertenencia
3 in [1, 2, 3]     # true (no soportado aún)

# Range
range(5)           # [0, 1, 2, 3, 4]
```

### Slicing

```python
items = [0, 1, 2, 3, 4, 5]

items[1:4]         # [1, 2, 3] (no soportado aún)
items[2:]          # [2, 3, 4, 5] (no soportado aún)
items[:3]          # [0, 1, 2] (no soportado aún)
```

---

## Diccionarios

Colección de pares clave-valor.

```python
person = {"name": "Ava", "age": 1}
config = {"host": "localhost", "port": 8080}

print(person["name"])  # Ava
person["email"] = "ava@example.com"
```

### Acceso

```python
data = {"a": 1, "b": 2}

print(data["a"])     # 1
print(data["c"])     # nil (no existe)

data["a"] = 10       # Modificar
data["c"] = 3        # Agregar
```

### Longitud

```python
len({"a": 1, "b": 2})  # 2
```

---

## Ejemplos

### Lista de diccionarios

```python
users = [
    {"name": "Alice", "age": 25},
    {"name": "Bob", "age": 30},
    {"name": "Charlie", "age": 35}
]

for user in users do
    print(user["name"] + " is " + str(user["age"]) + " years old")
end
```

### Matriz (lista de listas)

```python
matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]

print(matrix[1][1])  # 5
```

---

## Resumen

| Tipo | Ejemplo | Descripción |
|------|---------|-------------|
| nil | `nil` | Valor nulo |
| bool | `true`, `false` | Booleano |
| number | `42`, `3.14` | Entero o decimal |
| string | `"hello"` | Texto |
| list | `[1, 2, 3]` | Colección ordenada |
| dict | `{"a": 1}` | Pares clave-valor |
| function | `func() end` | Función |
| class | `class X() end` | Clase |
| instance | `X()` | Instancia de clase |