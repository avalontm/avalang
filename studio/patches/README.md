# patches/

## imguicolortextedit_interpolation.patch

`ImGuiColorTextEdit` (goossens fork, `studio/CMakeLists.txt` -> `FetchContent`)
no soporta:

1. Un color propio para el contenido de `{expr}` dentro de un f-string
   (`$"..."`) -- todo el string, interpolación incluida, se pinta con un
   único `Color::string`. No hay hook (`customTokenizer`) que se invoque
   *dentro* de un estado de string, solo a nivel de texto normal.
2. De paso, encontramos que el estado `inOtherString` de la librería (el
   que se usa para strings delimitados por secuencias custom, como nuestro
   `$"..."`) tiene un bug propio: pinta el contenido con `Color::comment`
   en vez de `Color::string`.

Este patch:
- Agrega `Color::interpolation` al enum y a las paletas dark/light por
  defecto (Ava Studio las pisa igual en `InitEditorPanel`).
- Hace que el estado `inOtherString` cuente el anidamiento de `{`/`}` y
  pinte ese rango con `Color::interpolation`; una `"` dentro de una
  interpolación abierta ya no cierra el string antes de tiempo.
- De paso corrige el bug de arriba (color `comment` -> `string`).

## Limitaciones conocidas (documentadas, no bugs sorpresa)
- El conteo de `{}` es por línea (se resetea en cada línea nueva). Un
  f-string con una interpolación que cruza un salto de línea sin cerrar
  (`{` en una línea, `}` en la siguiente) va a colorear mal la línea
  siguiente. No debería ocurrir con el uso normal de FSTRING en AvaLang.
- El contenido *dentro* de `{...}` se pinta entero con
  `Color::interpolation`, sin volver a tokenizar como código AvaLang (o
  sea, un string anidado dentro de la interpolación, `{f("x")}`, no se
  recolorea como string). Hacerlo bien requeriría un sub-lexer recursivo;
  fuera de alcance de este parche.

## Por qué un patch y no un fork
`studio/CMakeLists.txt` fija `GIT_TAG master` (ver PROGRESS.md/AvaStudio.md
sobre pin de versión). Un patch aplicado vía `PATCH_COMMAND` en
`FetchContent_Declare` es más fácil de mantener/revisar que forkear el
repo entero para este único cambio. Si algún día pinean un commit hash
específico y ese commit cambia estas líneas, el patch puede dejar de
aplicar limpio -- en ese caso, CMake va a fallar en el paso de
configuración con un error de `git apply`, no en silencio.

## imguicolortextedit_bold_keywords.patch

`ImGuiColorTextEdit` toma un único `ImFont` por llamada a `Render()`
(`font = ImGui::GetFont()`, ver `renderText()`) y lo usa para *todos* los
glyphs sin importar su `Color::...` -- no hay forma de pedirle "esto en
bold, esto en regular" sin tocar la librería, ni vía `Language` ni vía
`customTokenizer` (ese hook solo devuelve un `Color`, no un peso de
fuente).

Este patch agrega:
- `TextEditor::SetBoldFont(ImFont*)` y `SetBoldColors(std::initializer_list<Color>)`
  -- una segunda fuente opcional (`boldFont`) y un set de qué
  `Color::...` deben usarla (`boldColors`, un
  `std::array<bool, Color::count>`).
- En `renderText()`, el `else` final que llama a `font->RenderChar(...)`
  ahora elige `boldFont` en vez de `font` cuando `boldColors[glyph.color]`
  es `true`. Si `boldFont` es `nullptr` (no se llamó `SetBoldFont`) o el
  color no está marcado, el comportamiento es idéntico a upstream.

Ava Studio lo usa en `InitEditorPanel()` para que `keyword`/`declaration`
(if/while/func/true/false/nil/...) se rendericen con JetBrains Mono Bold
mientras el resto del código (strings, identificadores, comments) se
queda en la fuente regular -- ver `src/panels/editor_panel.cpp` y
`src/fonts/embedded_font.{h,cpp}` para de dónde sale esa segunda fuente.

### Por qué no "todo el código en bold" en vez de esto
Se evaluó y se descartó: bold en todo el texto del editor no necesita
patch (alcanza con un `PushFont`/`PopFont` de ImGui alrededor de
`editor.Render()`), pero un peso uniforme en todo el código cansa la
vista en sesiones largas y reduce el contraste entre letras parecidas a
16px (`rn` vs `m`, `cl` vs `d`). Bold selectivo en keywords da el énfasis
que se buscaba sin ese costo -- a cambio de necesitar este patch.

## Fix 2026-07: `Color` se movió dentro de la clase (GIT_TAG master)

Como se advertía arriba, al fijar `GIT_TAG master` un cambio upstream que
reordena el archivo puede romper el patch en silencio (aplica limpio con
`git apply` porque el contexto del hunk no cambió, pero el resultado no
compila). Eso pasó acá: `imguicolortextedit_bold_keywords.patch` insertaba
`SetBoldFont()`/`SetBoldColors(std::initializer_list<Color>)` justo
después de `Render()` (cerca de la línea 120 del header), asumiendo que
`Color` ya estaba declarado en ese punto de la clase. Upstream movió la
sección `enum class Color` más abajo (ahora después de
`GetLightPalette()`, ~línea 330+), así que para cuando el compilador
llegaba a `SetBoldColors(std::initializer_list<Color>)` el tipo `Color`
todavía no existía → `error C2065: 'Color': identificador no declarado`
(MSVC) al compilar `text_editor.vcxproj`.

Arreglo: el patch ahora inserta `SetBoldFont()`/`SetBoldColors()`
inmediatamente después de `GetLightPalette()` -- es decir, después de que
`enum class Color` y `class Palette` ya estén completamente declarados --
en vez de justo después de `Render()`. El resto del patch (el cambio en
`renderText()` de `TextEditor.cpp` y los campos `boldFont`/`boldColors`
en la sección `protected`, que ya vivían después de `Color`) no cambió.

Si esto vuelve a romperse por el mismo motivo, el síntoma es siempre el
mismo: `git apply` no falla, pero MSVC/GCC tira "identificador no
declarado" sobre `Color` en la línea donde se insertó `SetBoldColors`.
Solución: mover el bloque insertado a un punto de la clase que quede
*después* de la declaración de `enum class Color` en el header actual de
upstream (buscar `enum class Color : char` en `TextEditor.h`).

## imguicolortextedit_doc_comment.patch

`ImGuiColorTextEdit` solo conoce un color plano, `Color::comment`, para
todo lo que matchea `Language::singleLineComment` -- no hay forma de que
un bloque de doc-comment (`##` en AvaLang) se pinte distinto de un
comentario común (`#`), ni de resaltar un token específico (`@param
nombre`) dentro de ese bloque, sin tocar la librería.

Este patch agrega:
- `Color::docComment` y `Color::docParamTag` al enum y a las paletas
  dark/light por defecto (Ava Studio las pisa igual en
  `InitEditorPanel`, ver `src/panels/editor_panel.cpp`).
- `Language::docCommentPrefix` (AvaLang lo fija en `"##"`, ver
  `src/languages/avalang_language.cpp`), chequeado en
  `Colorizer::update` **antes** que `singleLineComment` -- tiene que ir
  antes porque `"##"` también matchea el prefijo más corto `"#"`, y el
  chequeo original que hubiera matcheado primero.
- Dentro de ese bloque, un escaneo palabra por palabra que busca
  `@param` y repinta ese tag (más el nombre de parámetro que le sigue,
  si hay uno) con `Color::docParamTag`, dejando el resto del bloque en
  `Color::docComment`.
- `Autocomplete::updateState` ahora también cuenta `docComment` y
  `docParamTag` como "estoy en un comentario" (antes solo miraba
  `Color::comment`), para no ofrecer autocompletado adentro de un
  bloque de doc-comment -- mismo comportamiento que ya tenía un `#`
  común.

### Por qué un prefijo nuevo (`docCommentPrefix`) y no reusar `singleLineCommentAlt`
La librería ya tiene `singleLineCommentAlt`, un segundo prefijo de
comentario de una línea -- pero lo pinta con el mismo `Color::comment`
de siempre (ver el branch correspondiente en `Colorizer::update`), no
da un color propio. Reusarlo hubiera significado parchear ese branch
igual, así que se agregó un campo dedicado con semántica más clara
(`docCommentPrefix`, en vez de "el prefijo alternativo, pero esta vez sí
coloreado distinto").

## Orden de aplicación
`CMakeLists.txt` aplica los dos patches en una sola llamada a
`git apply` (acepta varios archivos y los aplica en orden sobre el clon
limpio): primero `imguicolortextedit_interpolation.patch`, después
`imguicolortextedit_bold_keywords.patch`. No se pisan -- tocan zonas
distintas del archivo (colorizer/paleta el primero, `renderText()` y la
clase `TextEditor` el segundo) -- pero si algún día se agrega un tercer
patch, mantené el orden y probá `git apply patch1 patch2 patch3` a mano
sobre un clone limpio antes de subirlo (ver AvaStudio.md).
