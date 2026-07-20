# Coroutines

Soporte tipo generador: una función se convierte en coroutine con `coroutine(fn)`,
y se avanza paso a paso con `resume(co, ...args)`, pausando en cada `yield`.

## coroutine(fn)

Crea una coroutine a partir de una función (aún no la ejecuta).

```python
func gen()
    print("empieza")
    yield 1
    print("sigue")
    yield 2
    print("termina")
end

co = coroutine(gen)
```

**Parámetros:** 1 función
**Retorna:** un valor de tipo `Coroutine`, inicialmente en estado `Suspended`

## resume(co, ...args)

Ejecuta (o retoma) la coroutine hasta el próximo `yield`, o hasta que la función
termine.

```python
print(resume(co))   # empieza \n [1]
print(resume(co))   # sigue \n [2]
print(resume(co))   # termina \n nil
```

**Parámetros:** una `Coroutine`, más argumentos opcionales que se pasan a la
función en la primera llamada (o de vuelta al punto de `yield` en llamadas
posteriores)
**Retorna:**
- una **lista** con los valores del `yield` (`[v1, v2, ...]`) si la coroutine
  se suspendió — sí, aunque yields un solo valor, `yield 1` produce `[1]`, no `1`
- `nil` si la función terminó (llegó al final o a un `return` sin valor)

**Errores:**
- `attempt to resume a running coroutine` si ya está corriendo (recursión sobre
  sí misma)
- llamar `resume()` sobre una coroutine ya `Dead` no está bloqueado explícitamente
  hoy — simplemente vuelve a devolver `nil` de inmediato, sin re-ejecutar nada

## yield valor, ...

Statement (no expresión) que solo es válido dentro del cuerpo de una función
que se está ejecutando como coroutine. Suspende la ejecución en ese punto y
devuelve los valores dados a quien llamó `resume()`.

```python
func contador(desde)
    n = desde
    while true do
        yield n
        n = n + 1
    end
end

c = coroutine(contador)
print(resume(c, 10))   # [10]
print(resume(c))       # [11]
print(resume(c))       # [12]
```

**Limitación conocida:** `yield` solo suspende correctamente cuando aparece
directamente en el cuerpo de la función-coroutine. Si ocurre dentro de una
función anidada que la coroutine llama, no se propaga como pausa real — se
comporta como un `return` normal de esa llamada anidada. Escribir generadores
que deleguen `yield` a un sub-generador (`yield from`-style) todavía no está
soportado.

## Estados

| Estado | Significado |
|--------|-------------|
| `Suspended` | recién creada, o pausada en un `yield`, lista para `resume()` |
| `Running` | ejecutándose ahora mismo (dentro de la llamada a `resume()` actual) |
| `Dead` | la función terminó; nuevos `resume()` devuelven `nil` sin ejecutar nada |

## Notas de implementación

- `coroutine`/`resume` son funciones nativas globales
  (`src/builtins/builtin_coroutine.cpp`), no métodos — se llaman como
  `coroutine(fn)`, nunca como `fn.coroutine()`.
- El estado de vida (`Suspended` vs `Dead`) se decide con un flag explícito que
  se pone en `true` dentro del opcode `YIELD`, no infiriendo por si el
  resultado es `nil` (una función normal sin `return` también da `nil`, así
  que esa heurística daba falsos positivos).
- Cada coroutine mantiene su propio stack de frames (`Coroutine::frames`),
  separado del stack del código que la llamó; `resume()` intercambia
  (`swap`) el stack activo de la VM al entrar y lo restaura al salir o pausar.

## For loop sobre Coroutines

El for loop dinámico (`for x in iter do ... end`) soporta automáticamente
coroutines además de listas. El tipo se determina en tiempo de ejecución:

```python
func three_items()
    yield "a"
    yield "b"
    yield "c"
end

# Itera sobre la coroutine como si fuera una lista
for item in coroutine(three_items) do
    print(item)
end
# Output:
# a
# b
# c
```

**Implementación:** El compilador genera código que:
1. Llama a `type(iterable)` para obtener el tipo como string
2. Compara con `"coroutine"`
3. Si es coroutine → usa `CompileForCoroutine`
4. Si es lista → usa `CompileForList`

```python
# Ambos funcionan con el mismo for loop:
for x in [1, 2, 3] do    # Lista
    print(x)
end

for y in coroutine(gen) do  # Coroutine
    print(y)
end
```

---

Ver también: [Opcodes](opcodes.md#coroutines-implementado)
