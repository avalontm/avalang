# `data/` — tooltips y autocompletado de Ava Studio

Estos dos archivos son lo que el Code Editor muestra en sus tooltips
(`DrawKeywordHint` / `DrawParameterHint` en `panels/editor_panel.cpp`) y lo
que alimenta el popup de autocompletado. Se editan con cualquier editor de
texto plano — **no hace falta recompilar Ava Studio**: los cambios se ven
la próxima vez que abrís la app, o directamente en caliente si volvés a
Ava Studio después de guardar el archivo (se revisa la fecha de
modificación cada vez que se necesita mostrar un tooltip).

Si alguno de los dos archivos no existe o tiene un error de formato que
impide leerlo, Ava Studio usa una tabla de respaldo embebida en el
ejecutable (el mismo contenido que estos CSV traen de fábrica) — nunca se
queda sin tooltips, simplemente deja de ver tus cambios hasta que se
arregla el archivo.

**Importante:** ni `grammar/AvaLang.g4` ni el compilador leen estos
archivos, y estos archivos tampoco generan nada en la gramática. Si
agregás una palabra clave o una función nueva al lenguaje, hay que agregar
la fila acá A MANO para que el editor la conozca — son dos cosas
separadas que hoy no están conectadas.

## `keyword_docs.csv` — palabras clave (`if`, `while`, `try`, ...)

Columnas: `name,syntax,example,doc`

| Columna | Qué es | Obligatoria |
|---|---|---|
| `name` | La palabra clave tal cual (`if`, `while`, ...) | sí |
| `syntax` | El patrón abstracto de uso, con nombres de relleno como `condition` | sí |
| `example` | Un ejemplo concreto y copiable, con valores reales en vez de nombres de relleno | no, puede quedar vacía |
| `doc` | Una o dos frases explicando qué hace | sí |

Dentro de una celda:
- `\n` (barra invertida + n, dos caracteres) es un salto de línea. No se
  usan saltos de línea reales dentro de una celda.
- Si la palabra clave acepta más de una forma de escribirse (ej. `while`
  con o sin paréntesis), se ponen las dos separadas por `|||` (tres
  barras verticales) dentro de la misma celda de `syntax`.
- Si una celda tiene comas, comillas o necesita quedar clara, ponela entre
  comillas dobles (`"..."`); una comilla doble literal adentro se escribe
  duplicada (`""`).

Ejemplo de fila completa:

```csv
while,"while condition\n    ...\nend|||while (condition)\n    ...\nend","count = 0\nwhile count < 5\n    print(count)\n    count = count + 1\nend","Repeats the block for as long as condition stays true."
```

Dejar `example` vacío tiene sentido para palabras clave que solo se
entienden acompañando a otra (`then`, `in`, `as`, `catch`) — el ejemplo de
la palabra clave principal (`if`, `for`, `import`, `try`) ya las cubre.

## `builtin_signatures.csv` — funciones incorporadas (`print`, `len`, `range`, ...)

Columnas: `name,params,doc`

| Columna | Qué es | Obligatoria |
|---|---|---|
| `name` | El nombre de la función | sí |
| `params` | Lista de parámetros separados por `\|` (una sola barra) | sí, puede ser una lista vacía si no toma nada |
| `doc` | Qué hace, y cómo llamarla si tiene más de una forma | sí |

Un parámetro que empieza con `*` (ej. `*values`) indica "acepta cero o más
argumentos" — no hace falta ninguna columna aparte para eso, alcanza con
el asterisco en el nombre del parámetro, igual que en la firma real.

Ejemplo:

```csv
pow,base|exponent,Returns base raised to exponent (base ** exponent).
print,*values,"Prints every argument, converted with the same rules as str()..."
```

## `component_catalog.csv` — componentes del Toolbox (`Button`, `TextBox`, `CheckBox`, ...)

Columnas: `type,display_name,is_container,properties`

| Columna | Qué es | Obligatoria |
|---|---|---|
| `type` | El tipo tal cual lo usa AvaComponent (`button`, `stack`, ...) | sí |
| `display_name` | La etiqueta que se ve en el Toolbox (`Button`, `Stack`, ...) | sí |
| `is_container` | `true` o `false` -- si acepta que otros componentes se suelten adentro | sí |
| `properties` | Propiedades por defecto de una instancia nueva, como `key=default` separados por `\|` (una sola barra) | no, puede quedar vacía (containers como `column`/`row`/`stack`/`flex`) |

Ejemplo de fila completa:

```csv
button,Button,false,value=Button|enabled=true
```

Los valores por defecto de hoy son palabras simples, booleanos o cadenas
vacías -- si algún default necesitara contener literalmente `|` o `=`, esta
columna no lo soporta todavía (no hay escape definido para eso).

Si un `.avaui` referencia un `type` que no está en este CSV (por ejemplo,
de una versión más nueva de Ava Studio), `FindComponentType` devuelve
`nullptr` y el Designer Canvas cae al fallback de texto plano en vez de
fallar -- ver `panels/designer_canvas.cpp`.

## Cómo agregar una palabra clave o función nueva

1. Agregala primero donde corresponda en el lenguaje real: la gramática
   (`grammar/AvaLang.g4`) para una palabra clave, o
   `core/src/builtins/builtin_natives.h/.cpp` + `RegisterBuiltinGlobals`
   para una función.
2. Agregala también a `studio/src/languages/avalang_language.cpp`
   (`lang.keywords` o `lang.identifiers`) para que se coloree y entre al
   autocompletado — este paso sigue siendo C++, no está en el CSV.
3. Agregá la fila correspondiente acá (`keyword_docs.csv` o
   `builtin_signatures.csv`) para que tenga tooltip. Este paso sí es solo
   texto, sin recompilar.

Si en algún momento se arma un generador desde la gramática
(`tools/dump_docs.cpp` ya existe para volcar la tabla de fábrica a estos
CSV, pero no lee la gramática todavía), el paso 3 dejaría de ser manual
para las palabras clave. Ver `autocompletado-avalang.md` para el contexto
completo de esa idea.
