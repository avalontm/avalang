# Documentación de AvaLang

Lenguaje de scripting embebible con sintaxis tipo Python y semántica tipo Lua.

## Índice

### Conceptos Fundamentales
- [Tipos de Datos](types.md) - nil, bool, numbers, strings, lists, dicts
- [Operadores](operators.md) - Aritméticos, comparación, lógicos
- [Control Flow](control-flow.md) - if/elif/else, while, for, break, continue

### Características Avanzadas
- [Funciones](functions.md) - Definición, parámetros, return, closures
- [Clases](classes.md) - Definición, __init__, métodos, herencia con base()
- [F-Strings](fstrings.md) - Interpolación de expresiones con sintaxis `$"..."`
- [Excepciones](exceptions.md) - try/except/finally, raise, re-raise
- [Built-in Functions](builtins.md) - print, type, str, int, float, len, range, etc.
- [String Methods](string-methods.md) - .upper(), .lower(), .split(), .trim(), etc.
- [List Methods](list-methods.md) - .append(), .pop(), .insert(), .remove(), etc.
- [Dict Methods](dict-methods.md) - .keys(), .values(), .items(), .containsKey()
- [Coroutines](coroutines.md) - coroutine(fn), resume(co), yield
- [Opcodes](opcodes.md) - Conjunto de instrucciones del VM bytecode

---

## Uso Rápido

```python
# Ejecutar un script
ava_cli.exe script.ava
```

```python
# Hola mundo
print("Hello, World!")

# Variables
name = "Ava"
age = 1

# Condiciones
if age < 2 then
    print(name + " es un bebé")
end

# Funciones
func greet(person)
    print("Hola " + person)
end

# Clases
class Dog(name)
    __init__(self, name)
        self.name = name
    end
    
    bark(self)
        print(self.name + " dice Woof!")
    end
end

d = Dog("Buddy")
d.bark()

# Listas
nums = [1, 2, 3, 4, 5]
for n in nums do
    print(n)
end

# F-Strings (interpolación de expresiones)
name = "Ava"
print($"Hola {name}!")
x = 10
print($"La suma es {x + 5}")
```

---

## Gramática Básica

| Elemento | Ejemplo |
|----------|---------|
| Comentarios | `# esto es un comentario` |
| Variables | `x = 42` |
| Strings | `"hola"` o `'hola'` |
| F-Strings | `$"Hola {nombre}!"` |
| Listas | `[1, 2, 3]` |
| Dicts | `{"a": 1, "b": 2}` |
| Bloques | `end` (cierra if, while, for, func, class) |

---

## Palabras Clave

```
and       as        base      break     class     
continue  else      end       except    false     
for       func      if        in        nil       
not       or        raise     return    then      
true      try       while     finally
```

---

## Recursos

- **Código fuente:** `src/`
- **Scripts de prueba:** `scripts/`
- **Estado del proyecto:** `progress.md`