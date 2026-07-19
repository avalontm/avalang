# Dict Methods

Métodos disponibles en diccionarios. Se acceden con la sintaxis `dict.method()`.

## .keys()

Retorna una lista con todas las claves.

```python
data = {"name": "Ava", "age": 1, "type": "language"}
keys = data.keys()
print(keys)  # ["name", "age", "type"]
```

**Retorna:** Lista de strings (claves)

---

## .values()

Retorna una lista con todos los valores.

```python
data = {"name": "Ava", "age": 1}
values = data.values()
print(values)  # ["Ava", 1]
```

**Retorna:** Lista de valores

---

## .items()

Retorna una lista de pares [clave, valor].

```python
data = {"name": "Ava", "age": 1}
pairs = data.items()
print(pairs)  # [["name", "Ava"], ["age", 1]]
```

**Retorna:** Lista de listas [key, value]

---

## .length

Retorna la cantidad de pares clave-valor (propiedad).

```python
data = {"a": 1, "b": 2, "c": 3}
print(data.length)  # 3

empty = {}
print(empty.length)  # 0
```

---

## .containsKey(key)

Verifica si una clave existe en el diccionario.

```python
config = {"host": "localhost", "port": 8080}
print(config.containsKey("host"))  # true
print(config.containsKey("user"))   # false
```

**Parámetros:** key (string)
**Retorna:** `true` o `false`

---

## Ejemplos Combinados

### Iterar sobre claves

```python
data = {"a": 1, "b": 2, "c": 3}
for key in data.keys() do
    print(key)
end
# Output: a, b, c
```

### Iterar sobre valores

```python
data = {"a": 1, "b": 2, "c": 3}
total = 0
for val in data.values() do
    total = total + val
end
print(total)  # 6
```

### Iterar sobre pares

```python
data = {"name": "Ava", "type": "language"}
for pair in data.items() do
    print(pair[0] + " = " + str(pair[1]))
end
# Output:
# name = Ava
# type = language
```

### Verificar antes de acceder

```python
config = {"debug": true}

if config.containsKey("port") then
    print("Port: " + str(config["port"]))
else
    print("Using default port")
end
```

---

## Patrones Comunes

### Copiar diccionarios

```python
original = {"a": 1, "b": 2}
copy = {}
for key in original.keys() do
    copy[key] = original[key]
end
```

### Filtrar por claves

```python
data = {"name": "John", "age": 30, "email": "john@example.com", "phone": "123"}
needed = ["name", "email"]
filtered = {}
for key in needed do
    if data.containsKey(key) then
        filtered[key] = data[key]
    end
end
print(filtered)  # {"name": "John", "email": "john@example.com"}
```

### Contar valores

```python
data = {"x": 1, "y": 2, "z": 1}
counts = {}
for val in data.values() do
    key = str(val)
    if counts.containsKey(key) then
        counts[key] = counts[key] + 1
    else
        counts[key] = 1
    end
end
print(counts)  # {"1": 2, "2": 1}
```

---

## Acceso Directo vs Métodos

```python
data = {"name": "Ava", "age": 1}

# Acceso directo
print(data["name"])  # "Ava"

# Métodos
print(data.containsKey("name"))  # true
print(data.keys())                # ["name", "age"]
```