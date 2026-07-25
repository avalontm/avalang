# Ava Studio — Code Editor: fuente embebida + resaltado + autocompletado

Qué se hizo en esta sesión sobre el panel "Code Editor" (antes un
`ImGui::InputTextMultiline` plano) y cómo retomarlo.

## 1. Fuente embebida (JetBrains Mono)

- `tools/embed_font.py` convierte un `.ttf` a un header C++ (array de
  bytes). Se corrió una vez sobre `JetBrainsMono-Regular.ttf` →
  `src/fonts/jetbrains_mono_regular_ttf.h` (generado, no tocar a mano).
- `src/fonts/embedded_font.{h,cpp}`: `studio::LoadDefaultFont(16.0f)` carga
  ese array vía `ImFontAtlas::AddFontFromMemoryTTF` y lo pone como
  `io.FontDefault`.
- Se llama desde `main.cpp`, justo después de crear el `ImGuiIO` y **antes**
  de `ImGui_ImplOpenGL3_Init()` (el atlas se construye ahí).
- Resultado: la fuente queda compilada dentro de `ava_studio.exe`, sin
  `.ttf` ni `.dll` sueltos al lado del ejecutable.
- **Para cambiar de fuente o agregar un peso (Bold, etc.):** correr
  `python tools/embed_font.py <ttf> <header.h> <NombreArray>`, incluir el
  header nuevo, y llamar `AddFontFromMemoryTTF` de nuevo (ver
  `embedded_font.cpp` como plantilla). `FontDataOwnedByAtlas = false` es
  obligatorio porque el array es `static`, no hay que dejar que ImGui le
  haga `free()`.

## 2. Editor real (líneas, resaltado, autocompletado)

Se integró **[ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit)**
(la reescritura de Johan Goossens, la que el propio Dear ImGui recomienda
desde v1.92.8). Se trae por `FetchContent` en `CMakeLists.txt` (mismo
patrón que GLFW/ImGui: se compila como lib estática `text_editor` y queda
embebida en el `.exe`, no es una DLL externa).

- `src/panels/editor_panel.h`: `EditorState` ahora tiene un
  `TextEditor editor` en vez de `std::string text`. `GetText()`/`SetText()`
  son wrappers finos.
- `src/panels/editor_panel.cpp`:
  - `InitEditorPanel()` — se llama **una vez** al arrancar (`main.cpp`).
    Configura idioma, paleta, números de línea, indentado automático,
    match de llaves, y el callback de autocompletado.
  - `DrawEditorPanel()` — dibuja toolbar + `editor.Render(...)` cada frame.
- `src/languages/avalang_language.{h,cpp}`: la definición de sintaxis de
  AvaLang para el colorizer, sacada **directo de `grammar/AvaLang.g4`**
  (no inventada):
  - `keywords`: `if/then/elif/else/end`, `while`, `for/in`, `func`,
    `class`, `base`, `return/break/continue/pass`, `import/as/local`,
    `raise/try/catch/finally`, `yield`, `or/and/not`.
  - `declarations` (mismo color que keyword, categoría aparte): `true`,
    `false`, `nil`.
  - `identifiers` ("known identifiers", color propio): los nativos
    registrados en `core/src/builtins/builtin_init.cpp` (`print`, `len`,
    `range`, `sorted`, `abs`, etc.) — si agregás un builtin nuevo al VM,
    sumalo también acá.
  - comentario `#`, strings `'...'`/`"..."` con escape `\`.
  - **`getIdentifier`/`getNumber` son obligatorios**: si quedan en
    `nullptr` la librería NO tokeniza identificadores en absoluto (no hay
    default implícito) y ningún keyword se colorea, aunque esté bien
    escrito en el set. Están implementados a mano siguiendo
    `NAME : [a-zA-Z_][a-zA-Z_0-9]*` y `NUMBER : DIGIT+('.'DIGIT+)?`.
- Paleta: se parte de `TextEditor::GetDarkPalette()` y se pisan 3 entradas
  en `InitEditorPanel()` — `keyword`/`declaration` naranja, `comment`
  verde, `string` azul claro. El resto de la paleta (fondo, selección,
  números de línea, etc.) es la que trae la librería por defecto.
- Autocompletado: usa la clase `TextEditor::Trie` (fuzzy match) que trae la
  librería. `RebuildAutocompleteTrie()` la llena con keywords +
  declarations + identifiers del `Language`, más **todo identificador que
  aparezca en el documento actual** (`editor.IterateIdentifiers(...)`), y
  se reconstruye en cada cambio (`SetChangeCallback(..., /*delay=*/0)` —
  el archivo es chico, no hace falta debounce). El mismo callback también
  prende `state.dirty`.

## Cómo seguir

- **Errores de compilación en el editor:** la librería tiene
  `AddMarker(line, ...)` para pintar líneas con error (fondo rojo +
  tooltip). Sería el próximo paso natural: cuando `engine.RunScript()`
  devuelva un error con número de línea, llamar `editor.AddMarker(...)`
  antes del siguiente `Render()`.
- **Multi-archivo / pestañas:** hoy `EditorState` tiene un solo
  `TextEditor`. Si Explorer va a soportar abrir varios `.ava` a la vez,
  hay que pasar a un `std::vector<EditorState>` (o `map<path, EditorState>`)
  con una pestaña activa — cada `TextEditor` es independiente y pesado
  (undo history, cursors), no conviene compartir instancia.
- **Colores de números/puntuación:** la paleta trae valores por defecto
  para `Color::number` y `Color::punctuation`; no se tocaron. Si no
  convencen, se ajustan en el mismo bloque de `InitEditorPanel()`.
- **Ninguna build completa se corrió todavía**: todo se validó compilando
  `editor_panel.cpp`/`avalang_language.cpp`/`TextEditor.cpp` de forma
  aislada contra los headers reales (no hay Windows/vcpkg en el entorno
  donde se hizo este trabajo). Falta el build real con
  `build_studio.bat` para confirmar el link y ver el resultado final en
  pantalla.
- **Pin de versión:** `CMakeLists.txt` apunta `GIT_TAG master` del repo de
  Goossens (igual que ImGui apunta a `docking`). Si en algún momento se
  quiere reproducibilidad estricta, cambiar `master` por el hash de commit
  usado hoy.