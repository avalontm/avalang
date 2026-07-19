# String Methods

Métodos disponibles en strings. Se acceden con la sintaxis `string.method()`.

## .upper()

Convierte a mayúsculas.

```python
s = "hello world"
print(s.upper())  # HELLO WORLD
```

---

## .lower()

Convierte a minúsculas.

```python
s = "HELLO WORLD"
print(s.lower())  # hello world
```

---

## .split(separator)

Divide el string en substrings usando el separador.

```python
s = "apple,banana,cherry"
parts = s.split(",")
print(parts)  # ["apple", "banana", "cherry"]

text = "hello world"
words = text.split(" ")
print(words)  # ["hello", "world"]
```

**Parámetros:** separator (string)
**Retorna:** Lista de substrings

---

## .trim()

Elimina espacios en blanco al inicio y final.

```python
s = "  hello  "
print(s.trim())  # "hello"

text = "\t\ntest\n\t"
print(text.trim())  # "test"
```

---

## .contains(substring)

Verifica si el string contiene una subcadena.

```python
s = "Hello World"
print(s.contains("World"))  # true
print(s.contains("foo"))    # false
```

**Parámetros:** substring (string)
**Retorna:** `true` o `false`

---

## .replace(old, new)

Reemplaza todas las ocurrencias de `old` con `new`.

```python
s = "hello world"
print(s.replace("world", "Ava"))  # hello Ava

text = "aaa bbb aaa"
print(text.replace("aaa", "ccc"))  # ccc bbb ccc
```

**Parámetros:** old (string), new (string)
**Retorna:** String modificado

---

## .indexOf(substring)

Busca la posición de una subcadena.

```python
s = "Hello World"
print(s.indexOf("World"))   # 6
print(s.indexOf("foo"))     # -1 (no encontrado)
```

**Parámetros:** substring (string)
**Retorna:** Índice de la primera ocurrencia o `-1` si no se encuentra

---

## .startsWith(prefix)

Verifica si el string comienza con el prefijo.

```python
s = "Hello World"
print(s.startsWith("Hello"))  # true
print(s.startsWith("World"))  # false
```

**Parámetros:** prefix (string)
**Retorna:** `true` o `false`

---

## .endsWith(suffix)

Verifica si el string termina con el sufijo.

```python
s = "Hello World"
print(s.endsWith("World"))  # true
print(s.endsWith("Hello"))   # false
```

**Parámetros:** suffix (string)
**Retorna:** `true` o `false`

---

## .substring(start, end)

Extrae una porción del string.

```python
s = "Hello World"
print(s.substring(0, 5))   # "Hello"
print(s.substring(6))      # "World"
print(s.substring(6, 11)) # "World"
```

**Parámetros:** 
- start (number): Índice inicial
- end (number, opcional): Índice final

**Retorna:** Substring o string vacío si índices inválidos

---

## .length

Retorna la longitud del string (propiedad).

```python
s = "Hello"
print(s.length)  # 5
```

---

## Ejemplos Combinados

```python
email = "  UsEr@ExAmPlE.cOm  "

normalized = email.trim().lower()
print(normalized)  # user@example.com

username = "john.doe"
print(username.contains("@"))       # false
print(username.substring(0, 4))     # "john"

filename = "document.pdf"
print(filename.endsWith(".pdf"))    # true
print(filename.indexOf("."))        # 8
```