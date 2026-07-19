# Control Flow

## if / elif / else

```python
x = 1
if x == 1 then
    print("one")
end

if x < 0 then
    print("negative")
elif x == 0 then
    print("zero")
else
    print("positive")
end
```

---

## while

```python
i = 0
while i < 5 do
    print(i)
    i = i + 1
end
```

### While con break

```python
i = 0
while true do
    i = i + 1
    if i == 5 then
        break
    end
end
print(i)  # 5
```

### While con continue

```python
i = 0
count = 0
while i < 10 do
    i = i + 1
    if i % 2 == 0 then
        continue
    end
    count = count + 1
end
print(count)  # 5 (números impares)
```

---

## for

Itera sobre una colección.

```python
for item in [1, 2, 3] do
    print(item)
end
```

### For con range

```python
for i in range(5) do
    print(i)
end
# 0, 1, 2, 3, 4

for i in range(1, 6) do
    print(i)
end
# 1, 2, 3, 4, 5

for i in range(0, 10, 2) do
    print(i)
end
# 0, 2, 4, 6, 8
```

### For con strings

```python
for char in "Hello" do
    print(char)
end
```

### Break y continue en for

```python
for item in [1, 2, 3, 4, 5] do
    if item == 3 then
        break
    end
    print(item)
end
# Imprime: 1, 2

for item in [1, 2, 3, 4, 5] do
    if item % 2 == 0 then
        continue
    end
    print(item)
end
# Imprime: 1, 3, 5
```

---

## Return

Sale de una función.

```python
func find_first(items, target)
    for i in items do
        if i == target then
            return i
        end
    end
    return nil
end

print(find_first([1, 2, 3], 2))  # 2
print(find_first([1, 2, 3], 5))  # nil
```

---

## Ejemplos

### Búsqueda lineal

```python
func linear_search(items, target)
    for item in items do
        if item == target then
            return true
        end
    end
    return false
end

print(linear_search([1, 2, 3, 4, 5], 3))  # true
print(linear_search([1, 2, 3, 4, 5], 7))  # false
```

### FizzBuzz

```python
for i in range(1, 16) do
    if i % 15 == 0 then
        print("FizzBuzz")
    elif i % 3 == 0 then
        print("Fizz")
    elif i % 5 == 0 then
        print("Buzz")
    else
        print(i)
    end
end
```

### Tabla de multiplicar

```python
func print_table(n)
    for i in range(1, 11) do
        print(n + " x " + str(i) + " = " + str(n * i))
    end
end

print_table(5)
```

---

## Resumen

| Estructura | Sintaxis |
|------------|----------|
| if | `if cond then ... end` |
| elif | `elif cond then ...` |
| else | `else ...` |
| while | `while cond do ... end` |
| for | `for var in expr do ... end` |
| break | `break` |
| continue | `continue` |
| return | `return value` |