# Funciones

## Definición

Las funciones se definen con la palabra clave `func`.

```lua
func greet()
    print("Hello!")
end
```

---

## Parámetros

```lua
func greet(name)
    print("Hello " + name)
end

greet("Ava")  # Hello Ava
```

### Múltiples parámetros

```lua
func add(a, b)
    return a + b
end

print(add(3, 4))  # 7
```

---

## Return

```lua
func multiply(a, b)
    return a * b
end

result = multiply(5, 3)
print(result)  # 15
```

Sin `return`, la función retorna `nil`.

---

## Closures

Funciones anidadas pueden acceder a variables del scope externo.

```lua
func outer()
    x = 10
    func inner()
        print(x)
    end
    inner()
end

outer()  # 10
```

```lua
func make_adder(n)
    func add(value)
        return n + value
    end
    return add
end

add5 = make_adder(5)
print(add5(3))   # 8
print(add5(10))  # 15
```

---

## Funciones como Valores

Las funciones pueden asignarse a variables.

```lua
func double(x)
    return x * 2
end

my_func = double
print(my_func(5))  # 10
```

---

## Ejemplos

### Factorial

```lua
func factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

print(factorial(5))  # 120
```

### Fibonacci

```lua
func fib(n)
    if n <= 1 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

for i in range(10) do
    print(fib(i))
end
```

### Función de orden superior

```lua
func apply(fn, value)
    return fn(value)
end

func double(x)
    return x * 2
end

print(apply(double, 5))  # 10
```

---

## Resumen

| Característica | Sintaxis |
|----------------|----------|
| Definir | `func name() ... end` |
| Con parámetros | `func name(a, b) ... end` |
| Retornar | `return value` |
| Closures | Funciones anidadas acceden scope externo |
| Como valores | Asignar a variables |