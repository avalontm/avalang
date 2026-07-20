# Excepciones en AvaLang

AvaLang soporta un sistema completo de excepciones con `try`/`except`/`finally`.

## Sintaxis

```python
try
    # código que puede lanzar excepción
    risky_operation()
except <tipo> as <variable>
    # maneja la excepción
    print("Error:", variable)
except <tipo2> as <var2>
    # otro handler
finally
    # código que siempre se ejecuta
    cleanup()
end
```

## Opcodes Implementados

| Opcode | Descripción |
|--------|-------------|
| `TRY` | Push el handler al stack, jump a catch_pc si hay excepción |
| `TRY_END` | Pop del handler stack cuando el try body termina exitosamente |
| `CATCH` | Si hay excepción activa, clear flag y ejecuta el body; si no, skip |
| `RAISE` | Lanza una excepción, busca handler en el stack |

## Comportamiento

### Flujo de Control

1. **TRY** ejecuta: push handler con `catch_pc` al stack
2. **Try body** se ejecuta normalmente
3. Si el try body completa exitosamente:
   - **TRY_END** hace pop del handler
   - Jump al final (past except handlers)
4. Si ocurre una excepción (durante try body o native call):
   - El handler se poppea en RAISE
   - `try_had_exception_` se activa
   - PC salta a `catch_pc` (compiletime-patched, no runtime)
5. Cada **CATCH** verifica `try_had_exception_`:
   - Si true: clear flag y ejecuta el except body
   - Si false: salta al siguiente CATCH (fall-through)
6. Si ningún CATCH hace match, re-raise propagate al outer handler
7. **Finally** se ejecuta al final (不论 de éxito o excepción)

### Variables de Excepción

```python
try
    raise "algo salió mal"
except as e
    print(e)  #打印: algo salió mal
end
```

La variable capturada es un dict con:
- `type`: String con el tipo de excepción
- `message`: String con el mensaje

```python
try
    raise MyException("details")
except as err
    print(err["type"])    # MyException
    print(err["message"])  # details
end
```

### Re-raise

```python
try
    risky()
except as e
    print("log:", e)
    raise  # re-lanza la excepción actual
end
```

## Ejemplos

### Básico

```python
try
    x = 1 / 0
except as e
    print("Caught:", e)
finally
    print("Cleanup")
end

# Output:
# Caught:  division by zero
# Cleanup
```

### Sin finally

```python
try
    print("in try")
    raise "error"
except as e
    print("handled:", e)
end
print("done")
# Output:
# in try
# handled: error
# done
```

### Try anidado

```python
try
    try
        raise "inner"
    except as e
        print("inner caught:", e)
        raise  # re-raise al outer handler
    end
except as e
    print("outer caught:", e)
end
# Output:
# inner caught: inner
# outer caught: inner
```

## Estado de Implementación

- VM handler stack (`exception_handlers_`) ✅
- `try_had_exception_` flag para tracking de excepción activa ✅
- `pending_exception_` para native calls ✅
- Compiletime jump patching para catch_pc ✅
- Re-raise propagation ✅
- Fall-through skip past except handlers ✅
- finally execution ✅