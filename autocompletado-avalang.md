# Cómo funciona el autocompletado y los tooltips en Ava Studio

Este documento explica, con nombres de archivo y función reales, cómo está
armado hoy el sistema de ayuda del editor de código (autocompletado +
tooltips de sintaxis) para AvaLang, y responde la pregunta clave:
**¿si modifico la gramática o agrego algo nuevo, el editor lo detecta solo?**

**Respuesta corta: no, todavía no.** Todo lo que el editor "sabe" sobre
AvaLang está escrito a mano en tres tablas C++ dentro de `studio/`, separadas
del compilador real. Si cambiás la gramática o agregás un builtin y no tocás
esas tablas, el editor sigue funcionando pero con información desactualizada
o incompleta. Más abajo está el detalle de qué tocar en cada caso, y al
final una propuesta de cómo se podría automatizar de verdad.

---

## 1. Las tres piezas del sistema

Ava Studio no tiene un "language server" ni introspección contra el
compilador/VM real (`core/`). Lo que ves en el editor sale de tres tablas
independientes, todas del lado del **editor**, no del lenguaje:

| Pieza | Archivo | Qué guarda |
|---|---|---|
| **Keywords** (`if`, `for`, `while`, `try`, ...) | `studio/src/languages/keyword_docs.h/.cpp` | Sintaxis + descripción corta de cada palabra clave, para el tooltip que aparece mientras se escribe |
| **Funciones builtin** (`print`, `len`, `range`, ...) | `studio/src/languages/builtin_signatures.h/.cpp` | Firma (`print(*values)`) + descripción, para el "signature help" al llamar una función |
| **Resaltado de sintaxis + lista base de autocompletado** | `studio/src/languages/avalang_language.cpp` | Tres listas planas: `lang.keywords`, `lang.declarations` (`true`/`false`/`nil`), `lang.identifiers` (nombres de builtins) |

Además, `FunctionIndex` (`studio/src/languages/function_index.cpp`) escanea
en vivo el texto del buffer abierto (y sus `import`) buscando `func nombre(...)`,
así que **las funciones que vos mismo escribís en tu script sí se detectan
automáticamente**, sin tocar ningún archivo del editor. Ese es el único
punto del sistema que ya funciona "solo".

## 2. Cuándo se dispara cada tooltip

Todo pasa en `studio/src/panels/editor_panel.cpp`, dibujado a mano frame a
frame (ImGuiColorTextEdit no trae un panel de docs por sugerencia, solo una
lista plana de strings para el popup de autocompletado):

1. **`DrawParameterHint(tab)`** — si el cursor está dentro de los paréntesis
   de una llamada (`FindEnclosingCall` escanea la línea hacia atrás buscando
   el `(` que la abre), busca el nombre en `FunctionIndex` (local, importado
   o builtin) y dibuja firma + doc + qué parámetro está activo.
2. Si lo anterior no mostró nada, **`DrawKeywordHint(tab)`** — toma la
   palabra que termina justo en el cursor (`WordEndingAtCursor`, funciona
   incluso a medio escribir, ej. `"fo"`) y la busca en `KeywordDocs()`:
   - coincidencia exacta → muestra el tooltip completo.
   - coincidencia parcial mientras escribís → solo se muestra si el
     prefijo identifica **una sola** keyword posible (`"fo"` → solo puede
     ser `for`); si es ambiguo (`"i"` → `if`/`in`/`import`) no muestra nada
     todavía, para no confundir.
3. Aparte de estos dos tooltips, **`RebuildAutocompleteTrie(tab)`** arma el
   popup de sugerencias (Ctrl+Space / al tipear) juntando: keywords +
   declarations + identifiers de `avalang_language.cpp`, identificadores
   detectados en el buffer actual, y los nombres que `FunctionIndex`
   encontró. Se reconstruye en cada cambio del texto.

## 3. Qué es manual hoy (y qué tenés que tocar si cambiás algo)

Nada de esto lee `grammar/AvaLang.g4` ni `core/src/builtins/builtin_init.cpp`
en tiempo de build o de ejecución. Son tablas hermanas que hay que mantener
sincronizadas a mano. Guía rápida:

### Agregaste/cambiaste una keyword en la gramática (`grammar/AvaLang.g4`)
Tocá **los dos** archivos, o el editor ni la resalta ni la autocompleta, ni
muestra su sintaxis:
1. `studio/src/languages/avalang_language.cpp` → agregarla a `lang.keywords`
   (o `lang.declarations` si es un literal tipo `true`/`false`/`nil`) —
   sin esto no se colorea ni entra al trie de autocompletado.
2. `studio/src/languages/keyword_docs.cpp` → agregar una entrada en
   `KeywordDocs()` con su sintaxis y descripción — sin esto el editor la
   autocompleta pero `DrawKeywordHint` no tiene nada que mostrarle.

### Cambiaste la sintaxis de una keyword que ya existe (ej. `while` deja de aceptar paréntesis)
Solo hay que actualizar el campo `syntax` de esa entrada en
`keyword_docs.cpp` — el resto no cambia.

### Agregaste un builtin nuevo (`core/src/builtins/builtin_natives.h/.cpp` + `RegisterBuiltinGlobals`)
Tocá **los dos** archivos también:
1. `studio/src/languages/avalang_language.cpp` → agregarlo a `lang.identifiers`
   (así se colorea distinto y entra al trie).
2. `studio/src/languages/builtin_signatures.cpp` → agregar su `Make(...)`
   con parámetros y doc en `BuiltinSignatures()` — sin esto el signature
   help simplemente no aparece para esa función (no rompe nada, solo falta
   la ayuda).

### Nada que tocar
- Funciones (`func nombre(...)`) que el usuario escribe en su propio script,
  o que importa — `FunctionIndex::ScanText`/`ScanImports` las detecta solas
  en cada edición, incluida su doc si le ponés un bloque `##` arriba.

## 4. Cómo sería una versión realmente automática

Si en algún momento querés que esto deje de depender de mantener las tablas
a mano, hay dos caminos razonables:

- **Generar `keyword_docs.cpp`/`avalang_language.cpp` (la parte de
  keywords) a partir de `grammar/AvaLang.g4`** con un script que se corra
  como paso de build (o manual, tipo `scripts/gen_keyword_docs.py`): parsear
  las reglas del `.g4`, sacar los literales entre comillas de cada regla de
  `compoundStatement`/`smallStatement` y las líneas de comentario junto a
  cada una como fuente de la descripción. Esto cubriría el 90% pero seguiría
  necesitando texto humano para las descripciones (la gramática no explica
  qué hace cada cosa, solo la forma).
- **Introspectar `RegisterBuiltinGlobals` en tiempo de build** (o exponer
  una función tipo `ava_builtins_manifest()` desde el core, con nombre +
  aridad + doc en un formato tipo JSON) y generar `builtin_signatures.cpp`
  desde ahí — esto sí eliminaría por completo la posibilidad de que un
  builtin quede sin firma en el editor.

Ninguna de las dos está implementada todavía; hoy el sistema es
intencionalmente manual (está documentado así en los comentarios de
`builtin_signatures.h` y `keyword_docs.h`). Si querés, puedo armar el script
generador para alguno de los dos casos.
