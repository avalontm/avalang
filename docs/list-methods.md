# List Methods

Métodos disponibles en listas. Se acceden con la sintaxis `list.method()`.

## .append(item)

Agrega un elemento al final de la lista.

```python
nums = [1, 2, 3]
nums.append(4)
print(nums)  # [1, 2, 3, 4]
```

**Parámetros:** item (cualquier tipo)
**Retorna:** La misma lista (permite encadenar)

---

## .push(item)

Sinónimo de `.append()`. Agrega un elemento al final.

```python
stack = []
stack.push(1)
stack.push(2)
stack.push(3)
print(stack)  # [1, 2, 3]
```

**Parámetros:** item (cualquier tipo)
**Retorna:** La misma lista

---

## .pop()

Elimina y retorna el último elemento.

```python
nums = [1, 2, 3]
last = nums.pop()
print(last)  # 3
print(nums)  # [1, 2]
```

**Retorna:** El último elemento o `nil` si la lista está vacía

---

## .insert(position, item)

Inserta un elemento en la posición especificada.

```python
nums = [1, 2, 3]
nums.insert(1, 99)
print(nums)  # [1, 99, 2, 3]
```

**Parámetros:**
- position (number): Índice donde insertar
- item: Elemento a insertar

**Retorna:** La misma lista

---

## .remove(position)

Elimina el elemento en la posición especificada y lo retorna.

```python
nums = [10, 20, 30]
removed = nums.remove(1)
print(removed)  # 20
print(nums)     # [10, 30]
```

**Parámetros:** position (number)
**Retorna:** El elemento removido o `nil` si posición inválida

---

## .length

Retorna la cantidad de elementos (propiedad).

```python
nums = [1, 2, 3, 4, 5]
print(nums.length)  # 5

empty = []
print(empty.length)  # 0
```

---

## .contains(item)

Verifica si la lista contiene un elemento.

```python
nums = [1, 2, 3, 4, 5]
print(nums.contains(3))   # true
print(nums.contains(10))  # false
```

**Parámetros:** item (cualquier tipo)
**Retorna:** `true` o `false`

---

## Ejemplos Combinados

```python
# Simular una pila (stack)
stack = []
stack.push(1)
stack.push(2)
stack.push(3)

while stack.length > 0 do
    print(stack.pop())
end
# Output: 3, 2, 1

# Cola simple
queue = []
queue.push("first")
queue.push("second")
queue.push("third")
print(queue.pop())  # "first" (primer elemento)

# Filtrar elementos
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
even = []
for n in numbers do
    if n % 2 == 0 then
        even.append(n)
    end
end
print(even)  # [2, 4, 6, 8, 10]

# Buscar y remover
items = ["a", "b", "c", "d"]
if items.contains("c") then
    idx = items.indexOf("c")  # No existe indexOf en listas aún
    items.remove(2)
end
print(items)  # ["a", "b", "d"]
```

---

## Patrones Comunes

### Pila (LIFO)

```python
stack = []
stack.push(1)
stack.push(2)
x = stack.pop()  # 2
```

### Cola (FIFO)

```python
queue = []
queue.push(1)
queue.push(2)
x = queue.pop()  # 1
```

### Insertar al inicio

```python
items = [2, 3, 4]
items.insert(0, 1)  # [1, 2, 3, 4]
```

### Remover del inicio

```python
items = [1, 2, 3]
items.remove(0)  # [2, 3]
```