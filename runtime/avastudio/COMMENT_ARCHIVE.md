# Comentarios sustanciales removidos en Fase 0.5

Archivo generado automáticamente al correr la limpieza de comentarios de §7.1 -- agrupa, por archivo fuente, los comentarios que superaban el umbral de "no trivial" (explican una decisión de diseño, un motivo, una convención compartida, un TODO/FIXME/gotcha) antes de borrarlos del código. Pendiente: revisar a mano y podar lo que ya quedó redundante con este mismo plan, fusionar lo que aplica a varios archivos en una sola nota en vez de repetirla por archivo.

Total comentarios encontrados: 3591. Marcados como sustanciales: 498.

---

## `branding/logo_texture.cpp`

- **L3:**

```
// Pulls in the GL function/constant declarations (glGenTextures,
// glTexImage2D, GLuint, GL_TEXTURE_2D, ...) the same way main.cpp does:
// GLFW's own header brings in the platform's OpenGL header for us.
// imgui_impl_opengl3.h alone is NOT enough for this -- it only declares
// the ImGui_ImplOpenGL3_* backend functions, not raw GL symbols, which
// is why building this file with just that include failed with
// "GLuint: identificador no declarado" / "glGenTextures: no se
// encontró el identificador" under MSVC.
```

- **L13:**

```
// Fase 10 (image widget preview, designer_canvas.cpp): STBI_ONLY_PNG and
// STBI_NO_STDIO used to be defined here since this file only ever
// decoded the embedded PNG logo from memory. Both are gone now that
// this is the one TU providing the actual stb_image implementation for
// the whole binary -- designer_canvas.cpp needs the file-path loader
// (stbi_load(path, ...), which STBI_NO_STDIO strips out) and arbitrary
// image formats (which STBI_ONLY_PNG would strip out) to preview
// whatever `src` a user points an Image node at, not just PNG.
```


---

## `branding/logo_texture.h`

- **L5:**

```
// Decodes the embedded Ava Studio logo (baked into the exe from
// avastudio.png, see src/images/avastudio_logo_png.h) and uploads it as
// an OpenGL texture the first time it's called; every call after that
// just returns the cached texture id. Must be called after
// ImGui_ImplOpenGL3_Init() (it needs a live GL context) -- in practice,
// from inside the render loop, e.g. titlebar_panel.cpp.
//
// Returns 0 if decoding or upload ever failed, so callers can fall back
// to a plain drawn icon instead of showing a blank/garbage image.
```

- **L16:**

```
// Pixel dimensions of the decoded logo (0 until GetLogoTextureId() has
// been called at least once). Useful for preserving aspect ratio if the
// logo is ever drawn at a size other than square.
```


---

## `design/design_document.cpp`

- **L84:**

```
// Caught separately from std::exception below (ParseError IS-A
// std::exception, so order matters) so callers that want the
// structured position -- editor_panel.cpp's HighlightError, via
// out_info -- can get it instead of only the flattened what().
```


---

## `design/design_document.h`

- **L37:**

```
// sourcePath is optional: text coming straight from the editor may not
// be saved to disk yet, so there may be nothing to pass. When it IS
// known (LoadAvauiFile always has it), it's threaded into AvauiParser::
// Parse so a ParseError carries the right file (see Fase 2,
// PLAN_DIAGNOSTICOS_AVAUI.md).
//
// out_info (Fase 4) is filled in, when non-null, with the same
// message/line/column/source a ParseError carried -- structured, so a
// caller like editor_panel.cpp can hand it straight to HighlightError
// instead of only getting the flattened `out_error` string. Mirrors the
// outParseError out-param added to avahost's RenderAvauiDynamic* in
// Fase 3.
```


---

## `design/imgui_renderer.cpp`

- **L6:**

```
// Own texture cache, same pattern as designer_canvas.cpp's
// GetOrLoadImagePreview/ResolveImageSrcPath (Fase 10) -- those live in
// designer_canvas.cpp's anonymous namespace and aren't exposed outside
// that TU yet, so this keeps its own decoded-texture cache instead of
// reaching into another file's internals. Fase 4 (wiring this into
// designer_canvas.cpp) can decide whether to unify the two caches;
// until then a duplicate cache is a harmless, self-contained gap, not
// a correctness issue (both key off the same resolved path and would
// simply decode the same file twice, once per cache).
```

- **L147:**

```
// This used to fall through to OnDrawHtmlFragment (a no-op, same as
// GdiRenderer -- ImGui has no DOM to hand raw HTML to either), which
// is why a Link rendered as a completely blank box in the designer
// canvas: nothing else in the pipeline gave it a visual. `href`
// isn't a real navigation target at design time regardless (there's
// nowhere to navigate to inside the canvas), so it's unused here,
// same as `clickHandler`/`className` for OnDrawText above -- but the
// label itself needs to actually be drawn, styled to read as a link
// (link-blue text, underlined) so it's visible and positionable like
// every other control.
```


---

## `design/live_render_bridge.cpp`

- **L52:**

```
// Fase 2: pass the layout's own resolved path so a syntax
// error inside it (found via this project's `extends`)
// reports that file instead of an unlabeled line number.
```

- **L85:**

```
// See ui_pipeline_static_renderer.cpp / ProjectTheme::RegisterProjectFonts --
// must run before RenderTheme::Apply so AvaStudio's live preview matches
// what AvaHost actually ships (a component with an explicit fontName
// never triggers RenderTheme's own lazy registration).
```

- **L90:**

```
// See ui_pipeline_static_renderer.cpp -- same declared-style-file
// overlay (`style *` / `style <type>` blocks), so AvaStudio's live
// preview matches what AvaHost ships here too.
```


---

## `engine/engine_bridge.h`

- **L12:**

```
// Result of a compile+run cycle. Kept mainly so callers (main.cpp) can
// check success/failure without scanning the console; the Terminal panel
// itself now reads the full scrollback from EngineBridge::Console()
// instead of just the last result (see below).
```

- **L20:**

```
// Source position of the failure, 1-based (0 = unknown -- e.g. an
// error with no meaningful line, or success). Mirrors
// ava_last_error_line/ava_last_error_column (see avalang.h), read
// right after the failing ava_compile/ava_run call in RunScript().
// Only meaningful when !success.
```

- **L28:**

```
// File the failure actually happened in, mirroring
// ava_last_error_source (see avalang.h). Empty when the VM didn't
// know (e.g. a proto compiled before source_name tracking existed) --
// callers should fall back to the file that was run in that case,
// since a top-level error is in that same file. Non-empty and
// different from the run's own source_name means the error is inside
// an `import`ed module: main.cpp opens *that* file's tab so the
// highlighted line points at the real offending code instead of
// wherever the import statement happens to sit.
```

- **L40:**

```
// One line of the Terminal panel's execution console -- built to feel like
// a real terminal (stdout as the script prints it, plus markers for run
// boundaries and results) instead of a static "last result" label.
```

- **L45:**

```
// "> Run script.ava" separators
// one line of accumulated print() output
// compile or runtime error. AvaLang's compile errors are
// already formatted multi-line with the offending
// source line and a "^" column caret (see
// core/src/frontend/frontend_antlr.cpp), so this can
// itself contain embedded '\n's -- render with
// TextUnformatted, not Text, to keep them.
// final "OK" / "OK -> <value>" summary of a successful run
// echo of text submitted via the console's input box
```

- **L59:**

```
// Only meaningful when kind == Error. Mirrors RunResult::error_source/
// error_line/error_column (see below) -- carried per-line, not just
// on the RunResult, so a *previous* run's error line still knows
// where to jump to even after a later run has overwritten
// TerminalState::last_run. source empty + line 0 means "unknown",
// same convention as RunResult. The Output panel uses these to make
// the line clickable (see terminal_panel.cpp).
```

- **L71:**

```
// Thin C++ wrapper around the avalang C API (public/include/avalang.h).
// Owns one AvaVM for the lifetime of the studio session.
//
// This is intentionally NOT a generic "engine service" abstraction --
// it only wraps exactly what the Milestone 1 panels need. Grow it as
// the Designer/Toolbox/live-reload panels come online.
```

- **L85:**

```
// Compiles and runs `source` (source_name is only used for error
// messages, e.g. the file path). Does not persist any state between
// calls beyond the shared AvaVM globals. Every call appends to the
// console scrollback (see Console() below) rather than replacing it,
// so a session's whole run history stays visible -- like a real
// terminal, not a single-shot result panel.
```

- **L93:**

```
// Sets the base modules folder `import` falls back to when a module
// isn't found relative to the running script (see
// ava_vm_set_stdlib_path in avalang.h). Called once at startup with
// the persisted setting (util/settings.h), and again any time the
// user changes it via the Properties dialog -- takes effect on the
// next RunScript(), no restart needed. `path` empty means "use the
// default modules/ folder next to the executable" (see
// util::ResolveDefaultModulesDir()) -- this is where that's resolved,
// so the caller/settings file can stay portable and keep storing "".
```

- **L104:**

```
// The execution console: every print() from every RunScript() call
// this session, in order, plus the Run/error/result markers around
// them. print() output is captured live via ava_vm_set_print_callback
// (see the .cpp) instead of going to the process's stdout, which a
// windowed GUI app usually has no visible console for anyway.
```

- **L112:**

```
// Appends one console line directly, without going through
// RunScript()/the VM print callback. Used by the out-of-process
// script runner (panels/terminal_panel.cpp's PollScriptRun) to mark
// run boundaries ("Run <path>") and the final Success/Error summary
// for a script that actually ran in a separate ava_cli.exe process,
// so the Terminal panel's scrollback looks the same either way.
```

- **L124:**

```
// Splits `raw_text` on '\n' and appends each complete line as a
// Stdout ConsoleLine, buffering a trailing partial line (no newline
// yet) until the next call -- same line-buffering OnScriptPrint
// already does for the VM's own print callback, exposed here so the
// out-of-process runner's streamed stdout/stderr can reuse it
// instead of re-implementing the same buffering. Call
// FlushExternalOutput() once the producing process has exited to
// flush a final line with no trailing newline.
```

- **L135:**

```
// Called by the Terminal panel's console input box when the user
// presses Enter. Echoes the text into the console like a terminal
// would.
//
// IMPORTANT: this is scaffolding, not a working REPL input yet.
// There is no `input()` builtin registered anywhere in
// core/src/builtins, and there's a real design problem to solve
// before there can be one: ava_run() runs a script's top-level code
// synchronously start-to-finish on the calling thread, so there is
// no point at which a blocking input() call could hand control back
// to this GUI's event loop without freezing the whole window.
// AvaLang already has coroutines (ava_coroutine_resume, see
// avalang.h) which are the natural mechanism for this -- input()
// would need to suspend the coroutine instead of blocking, and the
// Output panel would resume it once SubmitConsoleInput() is called.
// Wiring that up is future work; for now this just queues the text
// (see input_queue_) for whenever that lands.
```

- **L154:**

```
// A read-only, host-side mirror of one AvaComponent -- built while we
// construct the demo tree below, so the Preview panel has something
// interactive to walk without re-implementing JSON parsing.
```

- **L169:**

```
// Builds a small fixed Component Tree via ava_ui_* (page > stack >
// text + button). This is a stand-in for "run a script that declares
// UI" until the `page`/`stack`/`button` builtins
// (core/src/ui/builtins.cpp) are wired into the VM -- see that
// file's header comment. Validates the Component/Property/Child/
// JSON path the Preview panel depends on.
```

- **L178:**

```
// AvaPrintFn trampoline (avalang.h's callback is a plain C function
// pointer, not a std::function, so it can't capture `this` directly).
```

- **L187:**

```
// print() always hands us a chunk ending in '\n' today (see
// builtin_print in core/src/builtins/builtin_natives.cpp), but this
// buffers/splits on '\n' rather than assuming that, so one console
// line is still produced per physical line even if that ever changes
// (e.g. a native that prints without a trailing newline).
```


---

## `fonts/embedded_font.cpp`

- **L20:**

```
// kJetBrainsMonoRegularTTF is a `static`/embedded array with program
// lifetime, not a heap buffer -- tell ImGui not to free() it when the
// atlas is destroyed.
```

- **L26:**

```
// GetGlyphRangesDefault() covers Basic Latin + Latin-1 Supplement,
// which includes the accented characters and punctuation needed for
// Spanish (ñ, á, é, í, ó, ú, ¿, ¡, ü, ...).
```


---

## `fonts/embedded_font.h`

- **L7:**

```
// Loads JetBrains Mono (bundled into the exe, see fonts/jetbrains_mono_regular_ttf.h)
// and installs it as ImGui's default font.
//
// Call this once, after ImGui::CreateContext() and before the first
// ImGui::NewFrame() -- in practice, right after `ImGuiIO& io = ImGui::GetIO();`
// in main(). Returns the loaded font (rarely needed by the caller; ImGui
// widgets pick it up automatically via io.FontDefault).
```

- **L16:**

```
// Loads JetBrains Mono Bold (bundled into the exe, see
// fonts/jetbrains_mono_bold_ttf.h) as an ADDITIONAL font -- unlike
// LoadDefaultFont(), this does NOT touch io.FontDefault, so the rest of
// the UI (menus, buttons, panels) stays on the regular weight. Only the
// Code Editor panel opts into it, via GetCodeFont().
//
// Call once, same timing constraints as LoadDefaultFont(): after
// ImGui::CreateContext() and before the first ImGui_ImplOpenGL3_Init()
// atlas build.
```

- **L27:**

```
// Returns the font loaded by LoadBoldFont(), or nullptr if it hasn't been
// called yet. editor_panel.cpp wraps TextEditor::Render() in
// PushFont(GetCodeFont())/PopFont() with this.
```


---

## `fonts/jetbrains_mono_bold_ttf.h`

- **L1:**

```
// Auto-generated from JetBrainsMono-Bold.ttf by tools/embed_font.py -- do not edit by hand.
// Regenerate with: python tools/embed_font.py <input.ttf> <output.h> <array_name>
```


---

## `fonts/jetbrains_mono_regular_ttf.h`

- **L1:**

```
// Auto-generated from JetBrainsMono-Regular.ttf by tools/embed_font.py -- do not edit by hand.
// Regenerate with: python tools/embed_font.py <input.ttf> <output.h> <array_name>
```


---

## `images/avastudio_logo_png.h`

- **L1:**

```
// Auto-generated from avastudio_logo_128.png by tools/embed_image.py -- do not edit by hand.
// Regenerate with: python tools/embed_image.py <input.png> <output.h> <array_name> <namespace>
```


---

## `languages/avalang_language.cpp`

- **L44:**

```
// "##" doc-comment blocks (used for function/param docs, see
// function_index.cpp) get their own color, checked before the
// plain "#" comment above.
```

- **L49:**

```
// grammar/AvaLang.g4: STRING accepts both quote styles, FSTRING
// ($"...") is the interpolated variant; both use the same escapes.
```

- **L55:**

```
// FSTRING goes through the "otherString" mechanism instead of the
// plain double-quote flag above: that's the one that lets the
// colorizer track `{expr}` interpolation nesting and give it its
// own color (Color::interpolation, see patches/) instead of
// painting the whole f-string in a single flat string color.
```

- **L79:**

```
// Built-in native functions registered in
// core/src/builtins/builtin_init.cpp -- colored as "known
// identifiers" so they read differently from user-defined names.
```


---

## `languages/avalang_language.h`

- **L7:**

```
// AvaLang's syntax rules for the Code Editor panel, mirroring the ANTLR
// grammar in grammar/AvaLang.g4: `then/do/end` block delimiters, `#`
// comments, single/double quoted strings (including f-strings), and the
// keyword set from the grammar's lexer rules.
//
// Kept in its own translation unit (rather than inline in editor_panel.cpp)
// so it's easy to find and update if the grammar changes.
```


---

## `languages/builtin_signatures.cpp`

- **L31:**

```
// Every builtin here is registered via VM::RegisterNative, which goes
// through the same SetGlobal() as a top-level `func` -- so all of
// them are overridable by a same-named local declaration.
```

- **L113:**

```
// data/docs/builtin_signatures.csv columns: name,params,doc
//  - params: pipe-separated raw parameter tokens as they'd appear in
//    FunctionSignature::params ("value", "*values", "base|exponent" for
//    two params). has_var_args is derived from any token starting with
//    "*", rather than a separate column, since the token already carries
//    that information.
//  - doc: plain sentence(s).
```


---

## `languages/builtin_signatures.h`

- **L10:**

```
// Signatures for every free-function builtin AvaLang registers globally
// (core/src/builtins/builtin_init.cpp's RegisterBuiltinGlobals ->
// core/src/builtins/builtin_natives.h/.cpp).
//
// The data lives in data/docs/builtin_signatures.csv, next to ava_studio.exe
// -- editable without recompiling. This is NOT introspected from a
// running VM (Ava Studio's editor has no such API today, see
// engine/engine_bridge.h), so the CSV is kept in sync by hand with that
// file: if a builtin is added/changed there, mirror it in the CSV too,
// or the Code Editor's autocomplete/parameter hints will drift out of
// date with what actually runs. If the CSV is missing or fails to parse,
// BuiltinSignatures() falls back to DefaultBuiltinSignatures(), which
// mirrors the CSV's shipped content.
//
// FunctionIndex::Rebuild() merges this table in *after* scanning the
// local buffer and its imports, and only for names not already found --
// so a script that declares its own top-level `func print(...)` still
// shows its own signature/doc instead of this one (see
// FunctionSignature::overridable and vm.cpp's SetGlobal: builtins and
// user globals share the same table, so a local declaration really does
// shadow the builtin at runtime, not just in this tooltip).
```

- **L33:**

```
// The hardcoded fallback table, used when data/docs/builtin_signatures.csv is
// missing or fails to parse. Also what tools/dump_docs.cpp writes out to
// bootstrap a fresh builtin_signatures.csv.
```


---

## `languages/class_index.cpp`

- **L12:**

```
// The set of AvaLang block-opening keywords that require a matching `end`
// (see grammar/AvaLang.g4: tryStatement, ifStatement, whileStatement,
// forStatement, funcDeclaration, classDeclaration). `then`/`elif`/`else`
// are NOT here on purpose -- they belong to the same `if` block and don't
// introduce their own `end`.
```

- **L36:**

```
// Same splitting rule as FunctionIndex's SplitParams (function_index.cpp):
// respects nested (), [], {} and quoted strings so a default value like
// `b=(1, 2)` or `c="a,b"` isn't split on its internal comma.
```

- **L86:**

```
// Parses a "@param name: desc" line (already stripped of "## " and
// trimmed). Accepts ':' or '-' as an optional separator, or none at all.
```

- **L104:**

```
// Same "##" doc-block convention as FunctionIndex (see ApplyDocBlock in
// function_index.cpp): everything that isn't an "@param" line becomes the
// summary in `sig.doc`; "@param name: ..." lines go into `sig.param_docs`.
```

- **L127:**

```
// Starting with `i` positioned right after a block-opening keyword's own
// header (past its NAME/params/condition/etc, at the first character of
// its body), scans forward -- skipping strings and comments -- counting
// nested try/if/while/for/func/class blocks until it finds the `end` that
// matches the block we started inside (depth starts at 1 for that block).
// On success, sets `body_end` to the index where that matching `end`
// keyword begins and advances `i` to just past it, returning true. On
// failure (no matching `end`, e.g. the file is mid-edit and unbalanced),
// restores `i` to its original value and returns false -- the caller
// abandons that one class/block rather than misparsing the rest of the
// file.
```

- **L182:**

```
// Parses `body` -- the text strictly between a class's header and its
// closing `end` (already extracted by ScanText via FindMatchingEnd) --
// looking for `func NAME(params) ... end` method declarations (same "##"
// doc-comment convention as FunctionIndex) and `this.NAME = ...`
// attribute assignments. Unlike FindMatchingEnd, this does NOT need to
// track block nesting itself: it only cares about locating `func`
// headers and `this` attribute writes wherever they occur in the body,
// the same best-effort, non-block-aware philosophy FunctionIndex::ScanText
// already uses for finding `func` anywhere in a buffer. AvaLang no tiene
// `self` (ver el chequeo explícito más abajo).
```

- **L194:**

```
// Records `attr_name` as an attribute if it isn't already known, or merges
// in `is_static`/`is_private` if it is -- but NEVER downgrades: a bare
// `this.x = ...` assignment inside a constructor must not erase the
// `static`/`private` flags that a class-body-level `static x = ...` /
// `private x = ...` declaration already set for the same name (see
// DISENO_visibilidad_clases_avalang.md §3.3 -- a `this.NAME =` write is
// just a normal use of an already-declared attribute, not a redeclaration).
```

- **L207:**

```
// At `i` (already advanced past whatever word was just read), tries to
// consume zero or more `static`/`private` keywords separated by inline
// whitespace, in any order (mirrors the grammar's `memberModifier*` from
// Fase A of DISENO_visibilidad_clases_avalang.md -- both orders are valid,
// e.g. `static private` or `private static`). Sets `is_static`/`is_private`
// accordingly. On the first word that ISN'T a modifier, rewinds `i` back to
// right before that word so the caller can reparse it normally (as `func`,
// an attribute name, or anything else) -- this function only ever consumes
// modifier keywords, never the thing that follows them.
```

- **L265:**

```
// `static`/`private` prefix (Fase E): consume any further
// modifiers, then expect either `func NAME(...)` (a modified
// method) or `NAME = expr` (a modified attribute declaration,
// the `attrDeclaration` from the design doc -- a bare
// class-body-level assignment, NOT `this.NAME =`).
```

- **L319:**

```
// Malformed `static`/`private func` header (no
// matching parens, etc.) -- fall through and
// reparse from the modifier keywords as plain
// identifiers, same "abandon and resync" policy as
// the rest of this best-effort scanner.
```

- **L325:**

```
// `NAME = expr` / `NAME += expr` / etc -- the
// `attrDeclaration` from the design doc (a bare
// class-body-level assignment, distinct from a
// `this.NAME = ...` write inside a method).
```

- **L347:**

```
// Neither `func NAME(...)` nor `NAME = expr` -- fall
// through: rewind to the first modifier keyword and let
// normal scanning continue (e.g. `static`/`private` used
// as a plain identifier somewhere unrelated, or a
// malformed declaration).
```

- **L389:**

```
// Constructor methods share the class's own name (see
// scripts/dog.ava: `class dog` / `dog()`) -- still indexed
// as a plain method like any other; the popup can filter
// it out later if that turns out to be noisy.
```

- **L400:**

```
// AvaLang solo tiene `this` -- no existe `self` en el lenguaje
// (confirmado: no aparece en grammar/AvaLang.g4 ni en el
// frontend). Si algún archivo tiene `self`, es resto de otro
// lenguaje/plantilla y no debe tratarse como acceso a atributo.
```

- **L424:**

```
// A plain `this.NAME = ...` write, with no
// modifier prefix, never upgrades an attribute to
// static/private on its own -- see RecordAttribute
// (it only ever ORs flags in, defaulting to false
// here, so a name already marked static/private via
// a class-body-level declaration stays that way).
```

- **L496:**

```
// Unmatched `end` (file mid-edit, or something we can't
// parse with confidence) -- abandon just this class and
// let the outer loop resume scanning from body_start.
```

- **L554:**

```
// Local buffer already scanned first in
// Rebuild(); for imports there's no "local
// wins" tie-break to preserve, so just index
// whatever this import defines.
```

- **L560:**

```
// TRANSITIVE, unlike FunctionIndex::ScanImports
// (which only follows one level): also walk
// THIS file's own imports, resolved relative to
// its own directory, so a class several
// imports away still surfaces in the popup.
```

- **L650:**

```
// "Own private member" -- visible only where the modifier's whole
// point is to still work: from inside the very class that declared
// it (see DISENO_visibilidad_clases_avalang.md §3.3 -- a private
// member inherited from a base class is NOT "own" for the child).
```

- **L664:**

```
// `this.` inside a method of `viewer_class` -- §5/§9: every
// public member, plus this class's own private members
// (never a private member inherited from a base class).
```

- **L671:**

```
// `NombreDeClase.` -- §5, third bullet: only `static`
// members. A private static is only offered from inside
// its own declaring class (e.g. Contador.validarLimite);
// a public static is always offered.
```


---

## `languages/class_index.h`

- **L12:**

```
// A method declared directly in a class body, together with the
// `static`/`private` modifiers scanned for it (see DISENO_visibilidad_clases_avalang.md
// §4/§5 and Fase E of TODO_autocompletado_miembros.md). Wraps
// FunctionSignature instead of adding these flags to it directly, since
// FunctionSignature is shared with plain module-level functions
// (FunctionIndex) which have no such concept.
```

- **L20:**

```
// `static func NAME(...)` -- shared, no `this`, called as `Clase.NAME(...)`
// `private func NAME(...)` -- not visible from outside the class
```

- **L24:**

```
// An attribute declared directly in a class body -- either as a bare
// `NAME = expr` / `static NAME = expr` / `private NAME = expr` class-body
// statement (the `attrDeclaration` from the design doc), or inferred from a
// `this.NAME = ...` assignment inside one of the class's own methods
// (AvaLang only has `this` -- there is no `self`). Best-effort, editor-side
// only: this mirrors the compiler's `is_static`/`is_private` AST flags
// (`core/src/ast/ast.h`), not a runtime guarantee.
```

- **L36:**

```
// A `class NAME [: base] ... end` block found in the current buffer or in a
// file reachable via `import` (see ClassIndex::Rebuild -- unlike
// FunctionIndex::ScanImports, imports here are followed TRANSITIVELY).
```

- **L44:**

```
// Methods declared directly in this class's body (does NOT include
// inherited ones -- see ClassIndex::FlattenedMembers for that).
```

- **L48:**

```
// Attributes declared or inferred directly in this class's body (does
// NOT include inherited ones -- see ClassIndex::FlattenedMembers).
```

- **L53:**

```
// One member surfaced to the autocomplete popup -- either a method (has a
// non-null `signature`) or an attribute (name only).
```

- **L64:**

```
// Which side of a `.` the popup is being asked to fill in for -- drives the
// three filtering rules from DISENO_visibilidad_clases_avalang.md §5 / §9.
```

- **L67:**

```
// `variable.` where `variable` is an inferred instance, from outside the class
// `this.` written inside a method of `viewer_class`
// `NombreDeClase.` -- direct access to the class object
```

- **L72:**

```
// Editor-side, best-effort index of `class` declarations in AvaLang. Mirrors
// FunctionIndex in spirit: NOT the compiler's AST, just a text scan good
// enough to drive autocomplete and member lookup ("dog." -> members of
// class dog). Does not validate syntax and silently ignores anything it
// can't parse with confidence.
```

- **L79:**

```
// Re-scans `text` (the buffer currently open in the editor) and follows
// every `import a.b.c [as alias]` it finds, TRANSITIVELY -- if the
// current file imports `a` and `a` imports `b`, classes defined in `b`
// are indexed too, same as a real IDE's project-wide symbol search (not
// just one level, unlike FunctionIndex::ScanImports). Import cycles are
// guarded against via a visited-files set.
```

- **L94:**

```
// All members of `class_name`, INCLUDING everything inherited through
// its base-class chain. A member declared locally wins over one with
// the same name inherited from a base class (override shadows parent),
// matching normal OOP lookup order. Returns an empty vector if
// `class_name` isn't in the index, or if a base-class name in the chain
// can't be resolved (the chain simply stops there -- no error).
```

- **L102:**

```
// Applies the three popup-filtering rules from
// DISENO_visibilidad_clases_avalang.md §5/§9 to a member list already
// produced by FlattenedMembers. Does not itself figure out `kind` or
// `viewer_class` -- that's the editor-context-detection job tracked as
// Fase 3 of TODO_autocompletado_miembros.md; this is just the filter.
//
//   kInstance   -> keep members with is_private == false (static or not).
//   kThis       -> keep members that are not private, OR that ARE
//                  private but declared_in == viewer_class (a class's
//                  own private members are visible from its own
//                  methods; a private member inherited from a base
//                  class is not -- see §3.3/§9's table).
//   kClassName  -> keep only is_static == true members. Among those,
//                  a private static is only kept if declared_in ==
//                  viewer_class (e.g. `Contador.validarLimite` is only
//                  offered while completing from inside Contador
//                  itself); public statics are always kept.
//
// `viewer_class` is the class whose method body the cursor is currently
// inside, or "" if it isn't inside any method (e.g. top-level script
// code) -- in that case every private member is excluded, since there
// is no class to "be inside of".
```

- **L131:**

```
// Parses every `class NAME [: base] ... end` block in `text` and stores
// it in classes_ with `source_file`. If the name already exists, does
// NOT overwrite -- like FunctionIndex, Rebuild() always scans the local
// buffer first, so "local wins" is automatic.
```

- **L137:**

```
// Finds every `import a.b.c [as x]` in `text` and, for each one,
// resolves + reads + scans the corresponding .ava file -- then recurses
// into THAT file's own imports too (see Rebuild's comment on transitive
// resolution).
```

- **L144:**

```
// Mirrors FunctionIndex::ResolveImportPath (same module search rule:
// <dir>/a/b/c.ava, then <dir>/a/b/c/index.ava).
```


---

## `languages/function_index.cpp`

- **L28:**

```
// Divide el texto crudo entre el '(' de una función y su ')' de cierre en
// parámetros individuales, respetando (), [], {} anidados y strings, para
// no partir en la coma interna de algo como `b=(1, 2)` o `c="a,b"`.
```

- **L78:**

```
// Parsea una línea "## @param nombre: descripción" (ya sin el "## " y
// trimeada). Acepta ':' o '-' como separador opcional, o ninguno
// ("@param nombre descripción"). Devuelve false si no hay un identificador
// válido justo después de "@param".
```

- **L98:**

```
// Reparte un bloque "##" ya juntado en pending_doc entre sig.doc (resumen
// general) y sig.param_docs (una entrada por cada línea "@param nombre:
// ..."), matcheando `nombre` contra ParamBaseName() de cada parámetro real
// -- así "## @param name: ..." documenta el param aunque en la firma
// tenga un default o sea *rest.
```

- **L125:**

```
// nombre "limpio" de un parámetro para matchear contra @param: quita el
// '*' de var-args y todo lo que venga después de un '=' (default value),
// así "@param items" matchea tanto "items" como "*items" o "items=[]".
```

- **L143:**

```
// "##" doc-comment convention: one or more consecutive lines starting
// with "##" immediately above a `func` become that function's summary,
// shown in the parameter hint tooltip the same way BuiltinSignatures()
// docs already are (see DrawParameterHint in editor_panel.cpp). Plain
// "#" comments are never picked up as doc -- otherwise any unrelated
// comment sitting above a function would silently become its "doc",
// which is worse than showing nothing. pending_doc is cleared by
// anything (code, a blank "#" comment, a string) other than more "##"
// lines or blank/whitespace, so the block must be truly immediately
// above the `func` it documents.
```

- **L158:**

```
// Saltar comentarios (# hasta fin de línea) y strings para no
// confundir la palabra "func" si aparece dentro de ellos.
```

- **L306:**

```
// Builtins van al final y solo rellenan huecos: si el script ya
// declaró (o importó) algo con ese nombre, esa entrada se queda tal
// cual -- ver el comentario sobre "override" en builtin_signatures.h.
```


---

## `languages/function_index.h`

- **L14:**

```
// texto crudo de cada param: "a", "b=1", "*rest"
// "" = buffer actual/sin guardar
// "nombre(a, b=1, *rest)" precalculado para UI
```

- **L21:**

```
// Descripción mostrada en el parameter hint. Para builtins viene de
// BuiltinSignatures() (ver languages/builtin_signatures.h). Para
// funciones de usuario, FunctionIndex::ScanText la llena a partir de
// un bloque de comentarios "##" (doble numeral) escrito inmediatamente
// arriba del `func` -- convención puramente del lado del editor, no
// requiere cambios en el parser/VM de AvaLang. Vacío si la función no
// tiene ese bloque encima.
```

- **L29:**

```
// Descripción por parámetro, extraída de líneas "## @param nombre: ..."
// dentro del mismo bloque "##" (ver ScanText). Clave = nombre del
// parámetro tal como aparece en `params` (sin '*' ni '=default').
// Vacío si el bloque no usa @param, o para builtins (que no lo
// necesitan -- su `doc` ya cubre todos los argumentos en una línea).
```

- **L35:**

```
// true si esta entrada vino de la tabla de builtins en vez de un
// `func` real del usuario/import.
```

- **L38:**

```
// Todos los builtins de AvaLang (print, len, ...) se registran como
// globals normales (VM::RegisterNative -> SetGlobal, ver vm.cpp), en
// la MISMA tabla que un `func` de nivel de módulo -- así que declarar
// tu propio `func print(...)` en el script simplemente pisa el
// builtin. true para todo lo que sale de BuiltinSignatures().
```

- **L46:**

```
// Nombre "limpio" de un parámetro crudo (ver FunctionSignature::params):
// sin '*' de var-args ni '=default'. "name" -> "name", "*rest" -> "rest",
// "b=1" -> "b". Usado para matchear contra las claves de param_docs.
```

- **L51:**

```
// Índice, del lado del editor, de declaraciones de función en AvaLang.
// NO es el AST del compilador -- es un escaneo de texto best-effort,
// suficiente para autocompletado/parameter hints. No valida sintaxis y
// simplemente ignora lo que no puede parsear con confianza.
```

- **L57:**

```
// Re-escanea `text` (el buffer abierto en el editor) y, por cada
// `import a.b.c [as alias]` que encuentre, intenta resolver y escanear
// también ese archivo en disco (ver ResolveImportPath). Los imports se
// siguen UN nivel -- se indexan los símbolos que ese módulo define, no
// los módulos que ese módulo a su vez importa.
```

- **L76:**

```
// Parsea cada `func NOMBRE(...)` en `text` (a nivel de módulo o dentro
// de una clase) y lo guarda en signatures_ con `source_file`. Si el
// nombre ya existe, NO se sobreescribe -- Rebuild() siempre escanea el
// buffer local primero, así que "local gana" es automático.
```

- **L82:**

```
// Encuentra cada `import a.b.c [as x]` en `text` y, para cada uno,
// intenta resolver+leer+escanear el .ava correspondiente.
```

- **L87:**

```
// module_path = {"a","b","c"} -> ruta de archivo, best-effort. Espeja
// ModuleResolver::ResolveModulePath (core/src/vm/module.cpp), que es
// lo que el runtime (__import__, ver builtin_natives.cpp) usa de
// verdad:
//   1. <current_file_dir>/a/b/c.ava
//   2. <current_file_dir>/a/b/c/index.ava (módulos-carpeta)
// No sigue los demás search_paths_ del resolver (stdlib, etc.) --
// solo resuelve relativo al archivo abierto, best-effort para el editor.
```


---

## `languages/keyword_docs.cpp`

- **L186:**

```
// data/docs/keyword_docs.csv columns: name,syntax,example,doc
//  - syntax: one or more forms joined by "|||", each with literal "\n"
//    for line breaks (see util/csv.h's UnescapeCell/SplitOn).
//  - example: a single concrete snippet, literal "\n" for line breaks.
//    May be empty.
//  - doc: plain sentence(s), no special escaping beyond CSV quoting.
```

- **L236:**

```
// Reload when: first call ever, or the CSV exists and its mtime moved
// forward since the last successful load (someone edited it while Ava
// Studio was open). A missing/unreadable CSV on first call falls back
// to the embedded defaults once and stops re-checking their mtime
// (there's nothing to check).
```

- **L249:**

```
// Only fall back if we've never had a good CSV load -- a CSV
// that briefly fails to parse mid-edit (e.g. an unbalanced
// quote while typing) shouldn't blow away a working table
// that's already loaded and in use.
```


---

## `languages/keyword_docs.h`

- **L9:**

```
// Syntax help for every AvaLang control-flow/statement keyword
// (languages::AvaLang()'s lang.keywords, mirroring grammar/AvaLang.g4).
//
// The data itself lives in data/docs/keyword_docs.csv, next to ava_studio.exe
// -- editable in any plain text editor, no recompile needed. This struct
// and KeywordDocs() are NOT derived from the grammar at build time (Ava
// Studio has no ANTLR introspection available to the editor), so the CSV
// is kept in sync by hand: if a keyword's grammar rule changes, mirror it
// there too, or the Code Editor's keyword hint tooltip (DrawKeywordHint in
// editor_panel.cpp) will drift out of date with what the parser actually
// accepts. If the CSV is missing or fails to parse, KeywordDocs() falls
// back to DefaultKeywordDocs() below, which mirrors the CSV's shipped
// content -- so a corrupted or deleted file never leaves the editor
// without tooltips, it just stops picking up hand edits until fixed.
```

- **L26:**

```
// One or more usage patterns, each a small multi-line block shown
// verbatim in the tooltip (e.g. "if expr then\n    ...\nend"). More
// than one entry means the grammar accepts more than one form (e.g.
// while's optional parens around its condition). Shown in a
// monospaced "code box" in the tooltip -- this is the abstract
// pattern, with placeholder names like `condition`/`expr`, not
// runnable code.
```

- **L35:**

```
// A single concrete, runnable snippet using real values instead of
// placeholders (e.g. for `if`, an actual `if age >= 18 then ... end`
// instead of `if condition then ... end`). Aimed specifically at
// someone who isn't a programmer and finds the abstract syntax
// pattern above harder to map onto real code than a worked example.
// Optional -- empty for keywords that only make sense paired with
// another one already shown (e.g. `then`, `in`, `as`, `catch`),
// where the owning keyword's example already covers it.
```

- **L45:**

```
// One or two sentences explaining what the keyword does, aimed at
// someone new to AvaLang -- shown under the syntax/example blocks the
// same way FunctionSignature::doc is shown under a function's
// signature (see BuiltinSignatures()/DrawParameterHint).
```

- **L52:**

```
// Keyed by keyword spelling ("if", "while", "for", ...). Every entry in
// languages::AvaLang()'s lang.keywords has an entry here. Re-checks
// data/docs/keyword_docs.csv's modification time on every call (cheap: a
// filesystem stat, not a re-parse, unless the timestamp actually
// changed) so editing the CSV and switching back to Ava Studio picks it
// up without restarting.
```

- **L60:**

```
// The hardcoded fallback table, used when data/docs/keyword_docs.csv is
// missing or fails to parse. Also what tools/dump_docs.cpp writes out to
// bootstrap a fresh keyword_docs.csv.
```


---

## `languages/member_access_resolver.cpp`

- **L23:**

```
// Mismo set que IsBlockKeyword en class_index.cpp (duplicado a propósito:
// ese está en un namespace anónimo de otra unidad de traducción, y este
// escaneo tiene una forma distinta -- avanza hacia adelante construyendo
// una pila de bloques abiertos en vez de buscar el `end` de un bloque ya
// conocido).
```

- **L33:**

```
// Un bloque `class`/`func`/`if`/`while`/`for`/`try` todavía abierto en el
// punto del buffer donde se cortó el escaneo. Solo los bloques `class`
// llevan nombre -- es lo único que FindEnclosingClass necesita.
```

- **L41:**

```
// Escanea `prefix` (todo el texto del buffer HASTA el cursor, ver
// ResolveMemberAccess) llevando una pila de bloques abiertos, y devuelve
// el nombre de la clase más interna que todavía esté abierta en ese punto
// -- "" si el cursor no está lexicamente dentro de ningún `class ... end`
// (código de nivel de módulo). Best-effort, mismo criterio de
// comentarios/strings que el resto de estos escáneres del lado del editor.
```

- **L103:**

```
// Detecta el patrón `identificador.parcial` justo al final de `before`
// (el texto de la línea actual hasta el cursor). `parcial` puede ser
// vacío (cursor recién después del '.'). Devuelve false si no hay '.' en
// esa posición, si no hay identificador antes del '.', o si el caracter
// anterior al identificador es OTRO '.' (encadenado `a.b.` -- fuera de
// alcance, Fase 6).
```

- **L151:**

```
// Solo `=` simple (Fase 2 dice literalmente "variable =
// ClaseConocida(...)") -- ni `==`, ni `+=`/`-=`/`*=`/`/=`, y
// nunca `this.NAME = ...` (eso ya viene excluido porque acá
// `var_name` es "this" y el siguiente caracter sería '.', no
// '=', así que este bloque simplemente no matchea).
```

- **L180:**

```
// Reasignado a algo que no se puede inferir -- invalidar
// cualquier tipo anterior conocido para este nombre en vez
// de dejar un mapeo viejo y ahora incorrecto (Fase 6:
// "variable reasignada a otro tipo más adelante").
```

- **L205:**

```
// Prefijo completo hasta el cursor (para saber en qué clase, si
// alguna, está el cursor lexicamente) -- ver FindEnclosingClass.
```

- **L223:**

```
// Orden de resolución: `this` primero, después una VARIABLE conocida,
// y solo si ninguna de esas aplica, un nombre de clase directo. Una
// variable tiene que ganarle a un nombre de clase homónimo -- el caso
// de referencia del TODO es literalmente `dog = dog()` seguido de
// `dog.`, donde "dog" es a la vez el nombre de la variable Y el de la
// clase; ahí se quiere el acceso de INSTANCIA (kInstance, todos los
// miembros públicos), no kClassName (que solo dejaría pasar los
// `static`, y "say()" no lo es).
```


---

## `languages/member_access_resolver.h`

- **L10:**

```
// Fase 2 de TODO_autocompletado_miembros.md -- inferencia de tipo de
// variable, best-effort, sin ningún tipo de scoping: un único mapa plano
// `variable -> nombre_de_clase` para todo el buffer.
//
// Solo registra una entrada cuando el lado derecho de un `variable = ...`
// es literalmente `ClaseConocida(...)` Y `ClaseConocida` es una clase que
// `ClassIndex` ya indexó (local o importada) -- así "variable = 5",
// "variable = otraFuncion()" o una clase que no existe simplemente no
// generan ninguna entrada, en vez de una adivinanza. Recorre el texto de
// arriba hacia abajo y CADA `variable = ...` que encuentra vuelve a
// evaluar la entrada de ese nombre (la sobreescribe si es un constructor
// conocido, o la borra si no lo es) -- por lo que "última asignación gana"
// sale solo, igual que pide la Fase 6 ("Variable reasignada a otro tipo
// más adelante en el archivo").
//
// No intenta resolver `this.attr = Clase(...)` (eso es un atributo, no una
// variable -- ver ClassIndex/RecordAttribute) ni parámetros de función ni
// nada que dependa de flujo de control: es deliberadamente ingenuo, igual
// que el resto de estos índices "del lado del editor".
```

- **L33:**

```
// "" si `variable` no tiene un tipo inferido (no se pudo inferir, o
// nunca se vio) -- el caller debe caer de vuelta al comportamiento
// actual en ese caso, nunca forzar una clase incorrecta.
```

- **L44:**

```
// Fase 3 -- a qué se le está pidiendo autocompletado.
//   kInstance  -> `variable.` (variable resuelta vía VariableTypeIndex)
//   kThis      -> `this.` escrito dentro del cuerpo de una clase
//   kClassName -> `NombreDeClase.` (acceso directo al objeto clase)
// Ver MemberAccessKind en class_index.h para el detalle de qué filtra
// cada uno vía ClassIndex::FilterForAccess.
```

- **L56:**

```
// Punto de entrada de la Fase 3: dado el texto completo del buffer, la
// línea 0-based donde está el cursor, y el texto de ESA línea desde su
// inicio hasta la posición del cursor (`text_before_cursor_on_line` --
// mismo substring que ya arma DrawParameterHint en editor_panel.cpp vía
// TextEditor::GetLineText/GetCursorPosition), intenta reconocer el patrón
// `identificador.parcial` justo antes del cursor y resolver `identificador`
// contra `class_index` / `var_types`.
//
// Nota (ver punto 1 de la Fase 3 en el TODO): el header vendorizado de
// TextEditor.h (FetchContent, fuera de este zip) no está disponible acá
// para confirmar qué expone `AutoCompleteState` más allá de
// `suggestions`/`searchTerm` (los dos únicos campos que editor_panel.cpp
// ya usa). Por eso esta función no depende de nada nuevo de
// AutoCompleteState: recibe el texto/posición ya obtenidos con los mismos
// métodos de TextEditor que DrawParameterHint usa (GetCursorPosition,
// GetLineText), que sabemos que existen porque ya están en uso.
//
// Devuelve false (y no toca `out`) si no se puede resolver con confianza
// -- identificador desconocido, encadenado (`a.b.`), `this` fuera de toda
// clase, etc. El caller debe caer de vuelta al trie global en ese caso
// (Fase 4), nunca mostrar un popup vacío.
```


---

## `main.cpp`

- **L458:**

```
// Folds the background "Run" (StartScriptRun, see terminal_panel.h)
// into the console/terminal_state.last_run as it streams in and
// once it finishes. Unconditional -- must run even while the
// Terminal panel tab is closed, see PollScriptRun's own comment.
```

- **L798:**

```
// Interactive Run (button/shortcut): out-of-process via
// ava_cli.exe, see StartScriptRun's comment in
// terminal_panel.h for why. This intentionally does NOT call
// perform_run() -- that stays in-process and untouched, since
// it's also what plugin_callbacks.run_project_on_main_thread
// uses (see below), and that path is synchronous by contract
// (plugins block on its return value).
```

- **L807:**

```
// A run is already in flight -- same "ignore, don't
// queue a second one" behavior perform_run() itself
// never had to think about (it always ran to
// completion before returning).
```


---

## `palette.h`

- **L5:**

```
// Ava Studio brand & syntax palette. Centralized so theme.cpp (UI chrome)
// and panels/syntax_highlight.cpp (editor tokens) always agree on the
// exact colors from the AvaLang brand guide.
```


---

## `panels/build_panel.cpp`

- **L32:**

```
// Same rationale as explorer_panel.cpp/editor_panel.cpp/designer_canvas.cpp's
// TrFormat: a locale CSV is meant to be safely hand-edited by a
// translator, so it shouldn't also have to double as a valid printf
// format string. Every caller below passes the already-substituted
// result through a literal "%s" format instead of handing Tr()'s raw
// value to an ImGui printf-style Text function directly.
```

- **L50:**

```
// --- auto-detection ------------------------------------------------
// SelfExecutableDir() / DetectAvaCliPath() used to live here; both moved
// to util/ava_cli_locator.h/.cpp (verbatim, same logic) so the new
// out-of-process script runner (panels/terminal_panel.cpp) can reuse
// them instead of duplicating this. Nothing else in this file changes.
```

- **L56:**

```
// A directory "looks like" the AvaLang repo root if it has the same two
// markers runtime/avacli/src/build_command.cpp itself checks for before
// accepting --repo-root -- reusing that exact check here means this
// panel never suggests a root ava_cli would reject anyway.
```

- **L66:**

```
// Walks upward from `start` (inclusive) looking for the repo root.
// Tried against both ava_studio.exe's own location (works for any
// project, as long as ava_studio.exe still lives inside the checkout --
// true for build_cli/ builds) and, as a fallback, the currently open
// project folder (works if the project itself lives inside the repo,
// e.g. samples/<name>).
```

- **L84:**

```
// A directory "looks like" a vcpkg checkout if it has vcpkg.exe (already
// bootstrapped) -- vcpkg.bat/vcpkg (POSIX) also exist pre-bootstrap, but
// there's nothing useful to point --repo-root-style detection at until
// bootstrap-vcpkg has actually run once.
```

- **L97:**

```
// Mirrors scripts/build/install.bat's own fallback order: an already
// exported VCPKG_ROOT wins (so a machine set up via install.bat/manually
// keeps working with zero configuration here), otherwise fall back to
// install.bat's own default clone target, "<repo_root>/vcpkg". If neither
// of those pan out (e.g. a packaged AvaLang install where the source repo
// isn't present), try next to ava_studio.exe itself -- covers an
// install_studio.bat-style layout where vcpkg/ was dropped alongside the
// executable instead of inside a full repo checkout.
```

- **L126:**

```
// in a shallow (depth-limited) walk, alphabetically -- good enough for
// "just point me at something" without scanning a huge tree.
```

- **L173:**

```
// IProcessStream is additive (see IProcessStream.h) -- backends
// that implement it (Windows) get live output as ava_cli
// actually produces it; backends that don't (Linux/Mac stubs)
// fall back to the old blocking Execute() below, same as before.
```

- **L218:**

```
// --- background vcpkg install ----------------------------------------
// Same steps as scripts/build/install.bat's own "3. vcpkg" /
// "4. antlr4 C++ runtime" / "4b. libcurl" sections, just run from Ava
// Studio instead of a terminal: clone (if not already present),
// bootstrap, then `vcpkg install antlr4:<triplet>` and
// `vcpkg install curl:<triplet>` -- antlr4 is what upgrades avalang.dll
// from the stub frontend to the real one, curl is what the ai_agent
// plugin needs to configure at all. Runs every step even after one
// fails (matching this panel's "show everything, let the person read
// the log" philosophy from the Build side) but only reports overall
// success if all of them exited 0.
```

- **L250:**

```
// Appends straight into state.vcpkg_log live (under the lock) so
// DrawBuildPanel() sees each step's output as it happens instead
// of only once the whole install finishes.
```

- **L323:**

```
// The OS the resulting binary will actually run on -- avapack does NOT
// cross-compile (the toolchain that compiles it is the toolchain of the
// machine running ava_studio.exe: CMake+MSVC on Windows, gcc/g++ on
// Linux/macOS), so unlike build_target below, this is never a user
// choice, just a read-only fact computed the same way
// build_command.cpp itself picks its toolchain. See §3.4.1 of
// PLAN_AVASTUDIO_IDE.md: the day avapack's Fase 8 adds real
// cross-compiling, this switches from a fixed label to a real combo
// with the host OS preselected -- the UI slot is already reserved here
// so that day doesn't need a panel redesign.
```

- **L345:**

```
// A labeled text field with an optional Browse button next to it,
// shared by every row in this panel so the layout (label above,
// input+button below, fixed gap) stays identical across all of them.
```

- **L355:**

```
// "##" + label makes the ID unique per row even though every row
// shows the same visible "Browse..." text -- otherwise ImGui hashes
// the literal label into the same ID and every Browse button in
// this window becomes the same widget.
```

- **L374:**

```
// Apply a completed native-dialog round trip before drawing, so the
// field shows the new value this same frame -- same pattern as
// settings_panel.h's browsed_folder.
```

- **L387:**

```
// --entry wants a path relative to --project (see
// build_command.cpp's --help), but the file dialog
// returns an absolute path -- convert it, falling back
// to the absolute path only if it turns out to live
// outside the project folder.
```

- **L419:**

```
// Read-only: see DetectedPlatformName()'s comment -- avapack can't
// target anything other than the host OS today, so there is nothing
// to choose here yet.
```

- **L427:**

```
// settings.build_target: "" (first run) and "desktop" both mean the
// same thing (see StudioSettings::build_target) -- only "barekernel"
// is a real second state, so a 2-item combo is enough.
```

- **L493:**

```
// --- Options -----------------------------------------------------
// Encryption/obfuscation/zero-disk/signing are Fase 3-7 concepts
// built for the desktop .exe path -- none of them apply to the
// BareKernel AppHeader flow (see build_command.cpp's --help: --target
// barekernel ignores --obfuscate/--zero-disk/--sign-*/--key-file
// outright), so the whole section is hidden rather than shown
// disabled -- nothing here would do anything for that target.
```

- **L575:**

```
// --- VCPKG_ROOT ------------------------------------------------
// Only needed if avalang.dll has to be recompiled from source
// inside build_pack\ (see runtime/avapack/README.md's Fase 0 --
// ava_cli build prefers a prebuilt avalang.dll/avalang_ui.dll
// next to itself and skips vcpkg entirely when it finds one).
// Set as the VCPKG_ROOT/AVA_VCPKG_TRIPLET environment variables
// on ava_studio.exe's own process right before launching
// ava_cli, the same way scripts/build/build_studio.bat/
// install.bat rely on VCPKG_ROOT already being exported --
// ava_cli.exe (a child process) inherits them automatically.
```

- **L617:**

```
// Live: forward whatever vcpkg has printed so far every frame,
// whether the install is still running or already finished.
```

- **L626:**

```
// Auto-fill so future builds (and future panel
// opens) point straight at it instead of
// re-running DetectVcpkgRoot's env-var/heuristic
// fallback every time.
```

- **L637:**

```
// NOTE (deliberately deferred, same criterion as i18n-7's
// Terminal StartScriptRun/PollScriptRun family): this line
// only announces text that is about to land in the live,
// "[vcpkg]"-prefixed stream forwarded to the Output panel
// via FlushLogToOutput() above -- not a standalone UI
// widget -- so it stays in English for now alongside that
// raw log content.
```

- **L691:**

```
// Export VCPKG_ROOT (+ the fixed triplet the rest of this repo's
// scripts assume, see install.bat's WHY comment) on ava_studio's
// own process before launching ava_cli -- CreateProcess with a
// null environment block (WinProcess.cpp) inherits the caller's
// env, so ava_cli's own std::getenv("VCPKG_ROOT") picks this up
// exactly as if it had been set in the shell ava_studio.exe was
// launched from. Only set when we actually have a value: an
// empty/missing vcpkg is fine (ava_cli falls back to a prebuilt
// avalang.dll if one sits next to it, see build_command.cpp).
```

- **L710:**

```
// Directory mode: a trailing separator tells ava_cli build to
// save the binary INSIDE this folder (named after `entry`)
// rather than treating the whole string as the final .exe's
// literal path -- see build_command.cpp's out_is_dir check.
```

- **L729:**

```
// Ignored by build_command.cpp for --target barekernel
// anyway (see its --help), but keep them out of the
// argv entirely for that target rather than relying on
// that -- these fields are also hidden from the UI
// above (§3.4.2), so nothing here can be non-default.
```

- **L747:**

```
// Mirrors build_command.cpp's directory-mode naming rule
// exactly (default_name = --entry's stem, "packaged" if
// that's somehow empty) -- so the success message below
// shows the real path ava_cli build will have written to,
// not just the --out folder.
```

- **L754:**

```
// Mirrors build_command.cpp's own suffix rule exactly: a
// barekernel AppHeader is always named *.exe (there's no
// separate litekernel executable convention), regardless
// of the host OS ava_studio.exe itself is running on.
```

- **L784:**

```
// Live: forward whatever the build has printed so far every
// frame, whether it's still running or already finished --
// state.log itself is appended to live by StartBuild()'s worker
// (via IProcessStream), so this just keeps Output in sync with
// what's already visible in this panel's own log box below.
```

- **L798:**

```
// The pass/fail header line itself still only goes out once,
// right after the build actually finishes (not live, since
// we don't know success/failure until then). Deliberately
// left in English, same criterion noted above for the vcpkg
// header line: it announces text destined for the
// "[build]"-prefixed live stream forwarded to Output via
// FlushLogToOutput() above, not a standalone UI widget.
```


---

## `panels/build_panel.h`

- **L13:**

```
// The "Build" panel -- a friendly front-end over `ava_cli build`
// (runtime/avacli/src/build_command.cpp), which in turn drives
// runtime/avapack (see runtime/avapack/README.md) to package an AvaLang
// project into a standalone .exe. This panel never talks to
// CMake/avapack directly; it only shells out to ava_cli, exactly like a
// developer would from the command line, so there is exactly one code
// path that knows how packaging actually works.
//
// Runtime (non-persisted) state for one Ava Studio session -- the build
// itself runs on a background std::thread (a packaging build can take
// anywhere from ~1s (incremental, prebuilt avalang.dll/avalang_ui.dll)
// to a minute+ (first --clean build), so running it inline on the UI
// thread would freeze the whole app). `mutex` guards every field below
// it; `building` is a separate atomic so DrawBuildPanel() can cheaply
// check "is a build in flight" every frame without taking the lock.
```

- **L32:**

```
// guarded by mutex -- stdout+stderr so far/at completion
// guarded by mutex -- a build has finished at least once
// guarded by mutex -- exit code of the last finished build
// guarded by mutex -- final .exe path, only valid if last_success
```

- **L37:**

```
// Guards against re-printing the "Build succeeded/failed" header line
// into the Output panel (panels/logs_panel.h) more than once per
// build -- DrawBuildPanel() flips this to true the frame has_result
// first becomes true, and StartBuild() resets it to false so the
// *next* build's header gets printed too. Only ever touched from the
// main thread (unlike the fields above, the background worker never
// sets this), so it does not need the mutex.
```

- **L46:**

```
// How much of `log` (byte offset) has already been forwarded to the
// Output panel, line by line -- guarded by mutex since the worker
// thread appends to `log` live (via IProcessStream, see StartBuild)
// while DrawBuildPanel() reads/advances this every frame, so the
// build's output shows up in Output in real time instead of only
// once the whole thing finishes. Reset to 0 alongside `log.clear()`
// in StartBuild().
```

- **L57:**

```
// --- vcpkg install (see StartVcpkgInstall in build_panel.cpp) ------
// Same background-worker shape as the build fields above, kept
// separate so an "Install vcpkg" run and a "Build Executable" run
// never share state -- they're independent operations, and a slow
// vcpkg bootstrap (first-time clone + antlr4/curl compile, several
// minutes) shouldn't block the Build button or vice versa.
```

- **L65:**

```
// guarded by vcpkg_mutex
// guarded by vcpkg_mutex
// guarded by vcpkg_mutex
// guarded by vcpkg_mutex -- only valid if vcpkg_last_success
// main-thread only, same convention as logged_to_output
// Same live-forwarding offset as log_forwarded_upto above, for vcpkg_log.
// guarded by vcpkg_mutex
```

- **L80:**

```
// Which field on the panel a "Browse..." click refers to -- main.cpp
// owns the actual native dialog (needs a GLFWwindow*, see
// platform/win32_titlebar.h), same round-trip pattern as
// settings_panel.h's out_browse_requested/browsed_folder. main.cpp opens
// a folder picker for kProjectDir/kOutputDir/kRepoRoot, and a file
// picker for kKeyFile/kAvaCliPath.
```

- **L99:**

```
// Set the one frame a "Browse..."/"Auto-detect" button that needs a
// native dialog was clicked -- "" (kNone)/otherwise.
```

- **L103:**

```
// Set the one frame any settings.build_* field actually changed (a
// browsed value was applied, a checkbox flipped, a text field lost
// focus after editing, etc.) -- main.cpp calls SaveSettings() when
// this comes back true, same convention as settings_panel.h's
// out_settings_dirty.
```

- **L111:**

```
// `explorer_root_dir`: the Explorer panel's currently open folder --
// used as the default for settings.build_project_dir when that field is
// still empty (first run), so the common case ("package the project I
// have open") needs zero configuration.
//
// `browsed_field`/`browsed_value`: normally kNone/"". The one frame
// after the person picks something from a native dialog opened because
// of a previous BuildPanelResult::browse_requested, main.cpp passes it
// back here so this panel can drop it into the right settings.build_*
// field.
//
// `log_bridge`: the Output panel's general log stream (panels/logs_panel.h)
// -- a running build's stdout+stderr gets forwarded here live, one line
// at a time as ava_cli actually produces it (see
// BuildPanelState::log_forwarded_upto), prefixed "[build]", in addition
// to staying in this panel's own scrollable log box, so you can watch a
// build progress from the Output tab too without needing this panel to
// be sized tall enough to show it.
//
// `p_open`: same ImGui::Begin p_open convention as every other panel
// (see settings_panel.h) -- pass the address of panel_open["Build###build"].
```


---

## `panels/builtin_panels.h`

- **L8:**

```
// Every built-in panel the View menu offers a show/hide checkbox for --
// same idea as VSCode's View menu listing Explorer, Search, etc. with a
// checkmark for each. Deliberately NOT included here:
//  - "Code Editor": the main editing surface, not something VSCode-style
//    apps let you hide entirely (there's nothing useful behind it).
//  - "Toolbox": only ever drawn while a .avaui tab is showing Design view
//    (see main.cpp) -- it isn't independently open/closed the way these
//    always-tabbed panels are.
// Both this array and main.cpp's `panel_open` map (which stores the
// actual runtime open/closed flag per name) key off these exact strings,
// which must also match the literal passed to each panel's
// ImGui::Begin() call -- see explorer_panel.cpp, properties_panel.cpp,
// preview_panel.cpp, terminal_panel.cpp, logs_panel.cpp.
//
// Each `id` carries a "###id" suffix (see PLAN_AVASTUDIO_IDE.md section
// 6.3): ImGui hashes only the part after "###" into the window's actual
// ID, so this same literal keeps matching the panel's Begin() window --
// and every std::find/map-key comparison against it (panel_open,
// closed_panels, DockBuilderDockWindow) -- regardless of active locale.
//
// `tr_key`/`fallback_label`: as of i18n-9, Explorer, Properties, Terminal,
// Logs, Build and Settings have been migrated to Tr() (see
// explorer_panel.cpp/properties_panel.cpp/terminal_panel.cpp/
// logs_panel.cpp/build_panel.cpp/settings_panel.cpp) -- their `tr_key`
// is set and the View menu below calls Tr(tr_key) for their label.
// Every other panel here still shows its plain hardcoded
// `fallback_label` until its own i18n-N phase migrates it, at which
// point it gets a tr_key too and fallback_label becomes empty, same as
// these six entries now. Exactly one of the two is ever non-empty for
// a given entry.
```


---

## `panels/designer_canvas.cpp`

- **L39:**

```
// Same rationale as explorer_panel.cpp/editor_panel.cpp's TrFormat: a
// locale CSV is meant to be safely hand-edited by a translator, so it
// shouldn't also have to double as a valid printf format string. Every
// caller below passes the already-substituted result through a literal
// "%s" format instead of handing Tr()'s raw value to an ImGui
// printf-style Text function directly.
```

- **L182:**

```
// Single shared visual language for the selection ring, used identically for
// containers and leaf controls (previously containers drew a fainter,
// thinner, more-rounded ring than leaves -- see designer_canvas.cpp session
// notes, "selection consistency" pass).
```

- **L192:**

```
// Single source of truth for "what rectangle does the selection system use
// for this node/region". Every place that needs to know the selection box --
// the hit-test area for hover/click, the hover ring, the selected ring, the
// resize handles -- calls this once and uses the result, instead of each
// site re-deriving its own padding/threshold logic (which is exactly how the
// hover/selected/hit-test mismatches from before crept in: three separate
// call sites, three subtly different rectangles). A future change to the pad
// amount or the compact threshold only has to happen here.
```

- **L206:**

```
// `has_own_padding`: true for containers, whose base rect is already
// chrome_p0/chrome_p1 -- a decorative box padded outward from the real
// layout rect (kRealContainerPadSide/kRealContainerPadTop) so the type chip
// and border have room to draw. Adding the usual selection pad ON TOP of
// that stacked two paddings, so a selected/hovered container's ring grew
// well past its own chrome and could bleed into a tightly-packed sibling
// right below it. Containers get pad=0 here (their chrome already supplies
// the breathing room); only leaf controls, which have no chrome of their
// own, get the extra pad.
```

- **L224:**

```
// Single draw call for the selection/hover ring -- color is the only thing
// that ever differs between "hovering" and "selected", so thickness/radius
// live here once instead of being repeated (and able to drift) at every
// AddRect call site.
```

- **L543:**

```
// Computed once and shared by the hover ring AND the selected ring below
// (previously hover drew at the tight p0/hit_p1 bounds while the selected
// ring drew at these padded sel_p0/sel_p1 bounds, and for containers the
// selected ring additionally started from the bigger chrome_p0/chrome_p1
// instead of p0/p1 -- so selecting a hovered node visibly "grew" the box
// instead of just changing its color, which is what was reported).
```

- **L573:**

```
// Unified selection chrome (09_DESIGNER_CANVAS_UX_PLAN.md "selection
// consistency" pass): containers and leaf controls now share the exact
// same ring color/opacity/thickness/corner-radius and the same padding
// rule, instead of containers getting a fainter, thinner, more-rounded
// ring with no handles. The only thing that still varies by size is
// whether resize handles fit -- not the ring itself.
```

- **L606:**

```
// Resize handles are now available for containers too, not just
// leaf controls -- width/height are plain properties on any node
// (see SetSizeProperty/TryGetNumericProperty above), so there was
// no functional reason a Panel/Row/Column couldn't be resized the
// same way a Button or Label already could.
```

- **L733:**

```
// Hit-test area now matches sel_p0/sel_p1 -- the same padded rect the
// hover/selected ring is drawn at -- instead of the tighter p0/hit_p1.
// That was the last piece of the mismatch: the ring already lined up
// between hover and selected, but the mouse only actually registered as
// "over the node" inside the narrower p0/hit_p1 box, so there was a dead
// strip (the padding) where you could see the ring's territory but
// hovering there did nothing.
```

- **L746:**

```
// Arrow, not Hand: this is a design-surface selection affordance, not
// a hyperlink/button action -- Hand here was misleading (matches the
// report that the cursor shouldn't change to a hand on hover).
```

- **L751:**

```
// Same sel_p0/sel_p1 rect the selected ring below uses -- hover
// and selected now trace the identical rectangle both for what's
// drawn AND for what actually responds to the mouse.
```

- **L803:**

```
// Only reserve a separate body hit-area when a header strip actually
// takes up its own space (the non-live-render placeholder mode). In the
// normal live-render mode header_reserves_space is false and hit_p1
// above already equals p1 -- so this used to fire anyway (guarded only by
// "header_bottom < p1.y", which is true for any container with height)
// and stamped a second, fully-overlapping InvisibleButton directly on top
// of "##node_hit_area" for every container. Because it was added later
// it silently won hover every time, which meant the container's
// right-click delete menu and its drag-to-move source -- both bound to
// "##node_hit_area" as "the last item" -- stopped firing, and clicks
// landed on whichever of the two duplicate buttons ImGui happened to
// resolve. That's the actual cause of containers selecting/behaving
// inconsistently, not just a cursor style issue.
```

- **L827:**

```
// Same helper/box the header/leaf hit-area uses -- any future
// change to padding, color, or ring style applies here too
// automatically instead of needing a matching edit.
```

- **L947:**

```
// "##CanvasDeleteConfirm" keeps the popup's real ID stable across
// locales, same pattern as explorer_panel.cpp's DrawDeleteConfirmPopup
// (see explorer.delete_title) -- the visible label is rebuilt from
// Tr() every frame, but OpenPopup()/BeginPopupModal() below always
// compute the same ID regardless of active locale.
```


---

## `panels/designer_canvas.h`

- **L14:**

```
// ImGui payload type for dragging an ALREADY-PLACED node to move/
// reorder it (Fase 4) -- as opposed to kToolboxDragDropId
// (toolbox_panel.h), which is for dropping a brand-new node from the
// Toolbox. Payload data is the moved node's IComponent::NodeId(),
// NUL-terminated, same convention as kToolboxDragDropId. Both the drag
// source and every drop target for this live entirely inside
// designer_canvas.cpp, so unlike kToolboxDragDropId this doesn't need
// to be shared with another panel's .cpp -- it's declared here (rather
// than file-local) purely so it's documented next to
// DrawDesignerCanvas instead of buried in the .cpp.
```

- **L26:**

```
// Draws the Design canvas for one open .avaui document: computes
// layout via the real avaui pipeline (LayoutEngine, see
// design/live_render_bridge.h -- as of Fase 2 (AVASTUDIO_AVAUI_
// MIGRATION_PLAN.md) this is the ONLY layout engine in use, no
// parallel fallback), draws each IComponent with its real widget look
// (SceneCommandWalker::Walk, Fase 4.2) plus
// a selection/hover/drop-zone overlay, and handles three interactions:
//
//   - Click a rectangle -> doc.selected_node_id is updated and a
//     PropertiesState is returned (same struct/shape DrawPreviewPanel
//     already returns for the read-only Preview tree, so whatever
//     wires this into main.cpp later can reuse that exact call site --
//     see 08_DESIGNER_VIEW_PLAN.md section 5.6 point 3). For a real
//     (non-synthetic) node this PropertiesState now has `editable =
//     true` plus `source_tab_id`/`selected_node_id` filled in (see
//     `tab_id` param below and properties_panel.h's PropertyEdit) so
//     main.cpp can write edits back into `doc` -- Fase 3.
//   - Drop a Toolbox payload (kToolboxDragDropId, see toolbox_panel.h)
//     onto a container node -> a new IComponent is appended to that
//     node's children (doc.tree->CreateComponent + parent->AddChild,
//     seeded with the catalog's default_properties) and doc.dirty is set.
//   - A `Componente()` call-site node (PascalCase type, see
//     design::ComponentResolver::IsComponentCall) is drawn as the
//     REAL resolved subtree of the imported component instead of an
//     empty box, when `project_root` is non-empty (see below).
//   - Drag an already-placed (real, non-synthetic) node and drop it
//     onto another node (kNodeMoveDragDropId, see above) -> Fase 4:
//     design::MoveNode reparents/reorders it. Dropping on the
//     top/bottom band of the target inserts as a sibling before/after
//     it; dropping on the middle band of a CONTAINER target moves it
//     inside as a new last child instead (see designer_canvas.cpp's
//     HandleDropTarget for the exact band math). The page root can't
//     be dragged (no parent to remove it from) and a synthetic
//     (resolved-import) node can't be dragged or dropped onto, same
//     restriction as Toolbox drops -- see the `synthetic` note below.
//   - Ctrl+Click a real node that has a "click" event bound (Anexo
//     9.17/9.18, 08_DESIGNER_VIEW_PLAN.md Fase 6) -> runs that handler
//     against the tab's cached state VM (design::InvokeHandler), which
//     already has `state` AND `doc.code_behind`'s functions bound
//     (design::BuildStateVM + design::BindCodeBehind). A plain click
//     (no Ctrl) still only selects, same as before -- this is
//     additive, not a separate Run mode/view. Mutating `state` this
//     way clears the tab's display-prop eval_cache so the canvas shows
//     the new values next frame; it does NOT touch `doc` itself (no
//     write-back, nothing to save/undo) and does NOT work on a
//     synthetic (resolved-import) node, same restriction as Fase 4/5.
//
// Wired into DrawEditorPanel/main.cpp since Fase 2 (view_mode
// dispatch, Toolbox conditional visibility, Ctrl+S -> SaveAvauiFile --
// all already done, verified against the actual repo in the 08 plan
// doc's session notes).
//
// `project_root`: the same fixed base directory every `import
// "components/x"` in `doc` is resolved against (see
// design/component_resolver.h's constructor comment on why this must
// be a single project root, not derived from wherever the file being
// edited happens to live) -- pass EditorState::project_root. Empty
// string (the default) disables resolution entirely: every component
// call-site node just draws as today's empty labeled box, same
// behavior as before this parameter existed -- callers that don't
// care about resolved components (tests, etc.) don't need to change.
//
// Resolution here is READ-ONLY / for display purposes only: the
// resolved tree is a throwaway copy (fresh NodeIds on every replaced
// subtree, see component_resolver.h), rebuilt from `doc.Root()` fresh
// every call -- never written back into `doc` itself. Clicking a
// node inside a resolved component still sets `doc.selected_node_id` and
// returns its PropertiesState for inspection, but it does NOT accept
// Toolbox drops or Properties edits (there's nowhere real in `doc` to
// write those -- see designer_canvas.cpp's `synthetic` check). Editing
// inside an imported component's own file is how you'd change it, same
// as the .NET prototype.
//
// Fixed in 08_DESIGNER_VIEW_PLAN.md section 9.10 (previously a known
// limitation, section 9.8): resolution now happens ONCE, for the whole
// tree, before ComputeLayout runs -- not node-by-node while drawing.
// That means a call-site's parent lays out against the component's
// REAL resolved height, not a fixed one-leaf-row placeholder; a tall
// Navbar() correctly pushes its siblings down instead of overflowing a
// slot sized before resolution.
//
// `tab_id`: EditorTab::id of the tab `doc` belongs to (see
// editor_panel.h) -- stamped into any PropertiesState this call
// returns (PropertiesState::source_tab_id) so main.cpp can find `doc`
// again by tab id when it gets a PropertyEdit back from
// DrawPropertiesPanel, without this function needing to know anything
// about EditorState itself. Default -1 (no caller other than
// editor_panel.cpp exists today, but a hypothetical one that doesn't
// care about write-back just gets PropertiesState::editable = false
// for every selection, same "safe default" pattern as `project_root`).
//
// `out_generated_handler`: Fase 5 (08_DESIGNER_VIEW_PLAN.md section 6)
// -- double-clicking a real, non-synthetic "button" node calls
// design::EnsureClickHandler(doc, ...) directly (this function mutates
// `doc` itself, same as a Toolbox drop or Fase 4's move), and when
// that happens this frame, `*out_generated_handler` is set to the
// handler's function name so the caller (editor_panel.cpp's
// DrawEditorPanel) can switch the tab to Code view and jump the caret
// to the generated stub -- same "double-click a Button on the form"
// flow VS6 had, documented as "Workflow Futuro" in AGENTS_STUDIO.md.
// Cleared to "" at the top of every call when non-null, so a caller
// can tell "nothing generated this frame" from "still holds a stale
// name from three frames ago" without maintaining that itself. Ignored
// entirely when null (the default) -- a caller that doesn't pass this
// just doesn't get the Fase 5 jump-to-code behavior, same safe-default
// spirit as `project_root`/`tab_id` above.
// `tab_id` also keys the Fase 6 state-VM cache AND (9.16) the
// ComponentResolver/resolved-tree cache (08_DESIGNER_VIEW_PLAN.md
// 9.14's pendiente 1 / "cachear state_vm/evaluación" and 9.8 punto
// 3's "cachear ComponentResolver/resolved_root"): before these passes
// DrawDesignerCanvas built a fresh AvaVM (design::BuildStateVM),
// re-resolved every `import`/`Componente()` call from disk, and
// compiled every visible display-prop expression from scratch on every
// single frame, discarding it all at the end of the same call. Now all
// of that is kept in a module-local cache in designer_canvas.cpp,
// looked up by `tab_id`, and only rebuilt together when `doc.dirty`
// flips or `project_root` changes (see designer_canvas.cpp's
// DrawDesignerCanvas body for the exact invalidation rule -- and its
// DesignerVmCacheEntry comment for the known cross-tab staleness
// trade-off this introduces for imported components). Only real for
// `tab_id >= 0` -- a caller that passes the default -1 (no real caller
// today besides editor_panel.cpp) gets the old per-call
// build-and-destroy behavior instead, since -1 isn't a safe cache key
// (every such caller would collide on the same slot).
// `log_bridge`: Fase 4 (AVASTUDIO_AVAUI_MIGRATION_PLAN.md) -- where a
// failed `BuildLiveRender` call and any node missing from its
// `uidToRect` (now `nodeIdToRect`) get logged (see designer_canvas.cpp's DrawDesignerCanvas
// body), in addition to the in-canvas banner both conditions already
// show regardless of this parameter. Pass main.cpp's session-wide
// `LogBridge` (see util/log_bridge.h) via EditorState::log_bridge.
// Default nullptr, same safe-default spirit as `project_root`/`tab_id`
// above -- a caller that doesn't pass one still gets the banner, just
// not the Output-panel log line.
```

- **L165:**

```
// Frees the cached Fase 6 state VM AND the 9.16 ComponentResolver/
// resolved-tree cache (see DrawDesignerCanvas's `tab_id` note above)
// for a tab that's about to disappear. Must be called once, with the
// closing tab's EditorTab::id, right before it's actually removed from
// EditorState::tabs -- otherwise that tab's AvaVM (created by
// design::BuildStateVM inside the cache, never destroyed by
// DrawDesignerCanvas itself anymore once cached) leaks for the rest of
// the process's lifetime, since nothing else ever revisits that
// tab_id's cache slot again. (The resolver/resolved-tree half of the
// cache doesn't need explicit freeing -- it's plain C++ objects, not a
// raw handle like the VM -- but it's dropped here too so a closed
// tab's slot doesn't linger holding memory for no reason.) Safe to
// call for any tab_id, including one that was never a .avaui tab /
// never cached anything (a no-op lookup miss) -- callers don't need to
// check EditorTab::is_avaui first.
```

- **L182:**

```
// Fase 10.1 (09_DESIGNER_CANVAS_UX_PLAN.md): el modo Preview dedicado
// que vivia aca (SetDesignerPreviewActive/IsDesignerPreviewActive/
// ResetDesignerPreviewState/GetDesignerPreviewLog/ClearDesignerPreviewLog
// + PreviewLogLine, Anexo 9.19) fue REMOVIDO -- perdio su proposito una
// vez que Fase 10 hace que el canvas dibuje los widgets reales SIEMPRE,
// sin depender de ningun toggle. Probar un handler `click` mientras se
// edita sigue siendo Ctrl+Click (Anexo 9.17/9.18, sin cambios, ver el
// comentario de DrawDesignerCanvas arriba) -- eso no era parte del modo
// Preview y no se toco. La infraestructura de bajo nivel que el modo
// Preview usaba para instalar sinks (VM::AlertSink/NavigateSink/
// PrintSink, ava_vm_set_alert_callback/ava_vm_set_navigate_callback,
// core/src/vm/vm.h) sigue existiendo en el core sin cambios -- lo que
// se elimino es la consola de UI que los mostraba, no el mecanismo en
// si; un futuro host puede seguir instalando esos sinks si hace falta
// mostrar ese output en otro lado.
```


---

## `panels/editor_panel.cpp`

- **L24:**

```
// Substitutes "%s" placeholders in Tr(key)'s value with `args`, in
// order, and returns the result -- same rationale as explorer_panel.cpp's
// TrFormat: a locale CSV is meant to be safely hand-edited by a
// translator, so it shouldn't also have to be a valid printf format
// string. Every caller below passes the already-substituted result
// through a literal "%s" format instead of handing Tr()'s raw value to
// an ImGui *printf-style Text function directly.
```

- **L1016:**

```
// Fase 4: same machinery already used for .ava errors -- jump to
// the offending line/column and mark it, instead of only showing
// the flattened message in the Code-mode banner below.
```

- **L1271:**

```
// "###code_editor" keeps the panel's real window ID stable across
// locales -- same trick as Explorer (see explorer_panel.cpp).
```

- **L1433:**

```
// "##EditorCloseConfirm" keeps this popup's real ID stable across
// locales -- same trick as Explorer's delete-confirm modal (see
// explorer_panel.cpp).
```


---

## `panels/editor_panel.h`

- **L18:**

```
// Which widget a tab's content area renders: the normal TextEditor
// buffer (Code, the default, every existing .ava/.md/etc. tab), or the
// Design canvas from designer_canvas.h (Design -- only ever set for
// tabs whose EditorTab::is_avaui is true). See DrawEditorPanel's tab
// content dispatch and 08_DESIGNER_VIEW_PLAN.md section 4.
```

- **L25:**

```
// One open buffer/tab in the Code Editor panel. Heap-allocated and owned
// via std::unique_ptr in EditorState::tabs (never stored by value in the
// vector) because autocomplete_config's callback captures `this` tab's
// address, and TextEditor::SetAutoCompleteConfig(&autocomplete_config)
// stores that pointer too -- both must stay valid for the tab's whole
// lifetime, which a reallocating std::vector<EditorTab> would break the
// moment a second tab is opened.
```

- **L33:**

```
// Stable identity for this tab's ImGui tab item, independent of its
// position in EditorState::tabs (which changes as tabs open/close) and
// independent of its display label (which changes on save-as/rename) --
// see DrawEditorPanel's use of "##tab%d" as the hidden ImGui ID suffix.
```

- **L42:**

```
// must outlive editor.SetAutoCompleteConfig(&this)
// func nombre(params) locales + de imports, para
// autocompletado con nombre real y parameter hints
// clases (locales + importadas, transitivo) para el
// autocompletado por miembros ("instancia." -> say())
// variable -> nombre_de_clase, best-effort
// (Fase 2 de TODO_autocompletado_miembros.md)
```

- **L51:**

```
// True only for the startup "Welcome" tab (see OpenWelcomeTab) -- it
// has no file and isn't a code buffer, so DrawEditorPanel renders a
// static welcome screen for it instead of the TextEditor widget.
```

- **L56:**

```
// True for any tab whose file_path ends in ".avaui" (detected in
// OpenFileInTab by extension, see 08_DESIGNER_VIEW_PLAN.md section
// 4) -- these get `design` populated instead of (or in addition to)
// `editor`, and default to view_mode == Design instead of Code.
// false for every other tab, which never touch `design` at all.
```

- **L65:**

```
// Set by OpenFileInTab if design::LoadAvauiFile failed to parse an
// existing .avaui file's contents -- `design` is left as a blank
// document (design::NewBlankAvauiDocument()) in that case rather
// than refusing to open the tab, same as opening a corrupt .frm in
// VS6 still opened *a* form, just an empty one. Empty string = no
// error (either not a .avaui tab, or it loaded/parsed cleanly).
// DrawEditorPanel shows this as a dismissable-by-editing banner
// above the Design canvas.
```

- **L78:**

```
// Display name for the tab label and the title bar: "Welcome" for the
// startup tab, the file's base name, or "Untitled" for a buffer that
// has never been saved.
```

- **L84:**

```
// Owns every open Code Editor tab (VSCode-style: opening a file that's
// already open focuses its existing tab instead of duplicating it;
// closing a tab leaves the others exactly as they were).
```

- **L92:**

```
// Tab a close was requested for but that has unsaved changes --
// DrawEditorPanel shows a Save/Don't Save/Cancel confirmation for it
// instead of closing immediately. -1 when no confirmation is pending.
```

- **L97:**

```
// Cross-frame command flags, mirroring the old single-buffer EditorState:
// set by the editor/titlebar this frame, consumed and cleared by
// main.cpp's global hotkey handling right after DrawEditorPanel().
```

- **L102:**

```
// Ctrl+W
// Ctrl+N
// set by the Welcome tab's "Open File..." action
// set by the Welcome tab's "Open Folder..." action
```

- **L107:**

```
// Set by DrawEditorPanel (cleared to nullopt at the top of every
// call, then filled in if the active tab is a .avaui in Design view
// and its canvas registered a click this frame) -- main.cpp reads
// this right after DrawEditorPanel() the same way it already reads
// DrawPreviewPanel's return value, so a click on the Design canvas
// updates the Properties panel exactly like clicking the Preview
// tree does.
```

- **L116:**

```
// The fixed root every `.avaui` file's `import "components/x"`
// resolves against (see design/component_resolver.h's constructor
// comment) -- set once by main.cpp right after
// ResolveWorkspaceDir(), same folder the Explorer is rooted at.
// Empty until main.cpp sets it, which DrawDesignerCanvas treats as
// "don't resolve components" (see designer_canvas.h) -- so a
// caller that never sets this just keeps the pre-resolver
// behavior instead of crashing on an empty base dir.
```

- **L126:**

```
// Fase 4 (AVASTUDIO_AVAUI_MIGRATION_PLAN.md): main.cpp's session-
// wide LogBridge (util/log_bridge.h), set once right alongside
// `project_root` above. DrawEditorPanel threads it straight through
// to DrawDesignerCanvas so a failed BuildLiveRender (or a node
// missing from its uidToRect) shows up in the Output panel, not
// just the in-canvas banner. Null until main.cpp sets it, which
// DrawDesignerCanvas treats as "don't log" -- same safe-default
// pattern as `project_root` being empty.
```

- **L139:**

```
// Tab `id` (EditorTab::id, not an index -- indices shift as tabs
// open/close) that DrawEditorPanel should force into focus this
// frame, e.g. because Explorer was clicked on a file that's already
// open in a background tab. -1 when nothing is pending. Only setting
// EditorState::active_tab isn't enough to actually switch the visible
// tab: ImGui's tab bar tracks its own "selected" tab internally and
// only reacts to programmatic selection via
// ImGuiTabItemFlags_SetSelected on the frame it's requested, so
// DrawEditorPanel consumes this field and clears it right after.
```

- **L155:**

```
// Opens `path` in a new tab, or focuses its tab if already open. Also
// used for brand-new untitled buffers when `path` is empty.
```

- **L162:**

```
// Creates Ava Studio's startup "Welcome" tab (VSCode-style landing page:
// quick actions, no code buffer) and focuses it. Only meant to be called
// once, right after InitEditorPanel, before any real file is opened.
```

- **L169:**

```
// Writes `tab`'s buffer to disk at its current file_path and clears its
// dirty flag. No-op if file_path is empty (untitled) or the file can't be
// opened for writing -- there's no error surfaced back to the caller in
// either case, same as SaveActiveTab's existing silent-failure behavior.
```

- **L175:**

```
// True if any non-Welcome tab has unsaved changes. Meant for the
// app-exit confirmation in main.cpp (GLFWwindow close, custom titlebar's
// X, and File > Exit all check this before actually closing).
```

- **L180:**

```
// Saves every dirty, non-Welcome tab that already has a file_path.
// Untitled dirty tabs (file_path empty) are left untouched -- there's
// nothing to write into without a Save As dialog, which needs the GLFW
// window handle this function doesn't have; the app-exit confirmation in
// main.cpp handles those separately, one Save As dialog at a time.
```

- **L187:**

```
// Highlights `line`/`column` (1-based; 0 = unknown, e.g. some runtime
// errors -- see core/src/common/ava_error.h) in the tab open on
// `file_path` as a compile/runtime error: tints that line's gutter and
// text red (palette::kError) with `message` as its hover tooltip, and
// moves the caret there so it's visible without hunting for it. No-op if
// `file_path` isn't open in any tab or `line` is 0. Called from
// main.cpp right after a failed EngineBridge::RunScript.
```

- **L197:**

```
// Clears any highlight set by HighlightError, on every open tab. Called
// before each run (a previous error's line shouldn't stay red after a
// successful retry) -- SetChangeCallback in InitTab also clears it
// per-tab as soon as that tab's buffer is edited.
```

- **L203:**

```
// Closes `index` immediately if its buffer is clean, or arms the
// Save/Don't Save/Cancel confirmation (resolved inside DrawEditorPanel)
// if it has unsaved changes. Safe to call with any valid index, including
// one that isn't the active tab (e.g. closing a background tab).
```

- **L209:**

```
// Closes the tab open on `path`, if any, immediately and without the
// usual unsaved-changes confirmation. Meant for when the file was deleted
// out from under the editor (e.g. Explorer's Delete) -- at that point
// there's nothing left on disk to save the buffer back to, so prompting
// "save before closing?" wouldn't make sense. No-op if `path` isn't open.
```

- **L216:**

```
// Updates the tab open on `old_path`, if any, to point at `new_path`
// instead -- for when the file was renamed out from under the editor
// (Explorer's Rename). The buffer/undo history/cursor are untouched, only
// EditorTab::file_path changes, so the tab's label (DisplayName()) picks
// up the new name on the next frame and any subsequent Save writes to the
// new location. No-op if `old_path` isn't open.
```

- **L224:**

```
// F7 / "View > Toggle Design View" -- flips `tab.view_mode` between
// Code and Design for a .avaui tab (no-op for any other tab, same as
// VS6 where F7 only meant something with a .frm open). See
// 08_DESIGNER_VIEW_PLAN.md section 4.
//
// Design -> Code: re-serializes `tab.design` (design::WriteAvauiText)
// into `tab.editor`'s buffer, so Code view always reflects the latest
// drag&drop/property edits made in Design view, not stale text from
// whenever the file was last opened or last toggled to Code.
//
// Code -> Design: re-parses `tab.editor`'s current text
// (design::ParseAvauiText) back into `tab.design`, so hand-edits made
// in Code view (e.g. to the `methods`/code-behind section) aren't lost
// when switching back. If the text fails to parse, the toggle is
// aborted (view_mode stays Code, tab.avaui_load_error is set and shown
// as a banner above the TextEditor) rather than silently discarding
// the last valid Design tree or the user's unparseable edit -- fix the
// syntax, then F7 again.
```

- **L244:**

```
// Draws the Code Editor panel (center dock): a VSCode-like tab strip --
// reorderable, closable, unsaved-changes dot -- above the active tab's
// editor (line numbers, AvaLang syntax highlighting, keyword/built-in
// autocomplete, member ("instancia.") autocomplete -- both the library's
// native popup via PopulateMemberSuggestions and the hand-drawn
// DrawDotCompletionPopup that covers the instant right after typing the
// '.' itself --, function parameter hints, and a keyword syntax tooltip
// while typing if/while/for/... -- see DrawKeywordHint in
// editor_panel.cpp). Also renders the close-confirmation modal when a
// dirty tab's close was requested.
```


---

## `panels/explorer_panel.cpp`

- **L16:**

```
// Substitutes the first "%s" in Tr(key)'s value with `arg` and returns
// the result -- used instead of handing a translator-editable Tr()
// string straight to ImGui's *printf-style Text functions as the format
// string itself: a locale file is meant to be safely hand-edited by a
// translator, so it shouldn't need to also be a valid printf format (a
// stray "%d" typo there would read a nonexistent va_arg). The caller
// still passes the *result* through "%s" (see DrawDeleteConfirmPopup/
// DrawDirectory below), so there's never a translator string in format
// position either way.
```

- **L56:**

```
// True if `child` is `parent` itself or lives somewhere underneath it.
// Compared path-component-by-path-component so "/" vs "\\" or a trailing
// separator don't matter. Used to (a) stop a folder being dragged inside
// its own subtree, and (b) clear the selection when the selected entry
// was inside a folder that just got deleted.
```

- **L70:**

```
// Moves `src_path` (file or folder) on disk into `dest_dir`, keeping its
// current name. Used by every drag-and-drop drop target below. Reports
// the move via `result.file_renamed` -- same field a manual Rename uses --
// so the caller's existing tab-retargeting logic (main.cpp ->
// studio::RenameTabPath) handles it identically either way.
```

- **L107:**

```
// Small colored square drawn inline before the label -- a lightweight
// stand-in for per-extension file-type icons (VSCode-style) without
// needing an icon font/texture atlas.
```

- **L151:**

```
// "Delete" needs an explicit Yes/Cancel confirmation -- unlike creating a
// file/folder, deleting one is destructive and can't be undone from
// Explorer, so a stray click shouldn't be able to remove a script outright.
// Actually removes the file from disk on confirm and reports it via
// `result.file_deleted` so the caller (main.cpp) can close its tab if the
// file happened to be open in the editor.
```

- **L158:**

```
// "##DeleteConfirm" keeps the popup's real ID stable across locales,
// same trick as the About/Plugins modals in titlebar_panel.cpp: the
// visible "¿Eliminar?"/"Delete?" part is rebuilt from Tr() every
// frame, but OpenPopup() and BeginPopupModal() below always compute
// it the same way in the same frame, so the two never disagree even
// if the locale changes while this popup happens to be open.
```

- **L186:**

```
// Split the popup's own width evenly instead of two fixed-size
// buttons floating at the left -- keeps them centered and
// balanced regardless of how long the filename made the window.
```

- **L192:**

```
// Destructive action gets a subtle red so it doesn't read as a
// plain "OK" -- Cancel stays the neutral default look.
```

- **L205:**

```
// Clear the selection if it was the deleted entry itself, or
// (for a deleted folder) anything that used to live inside it.
```

- **L223:**

```
// "Rename" pre-fills the popup with the file's current name (not its full
// path) so the user is just editing the base name, same as VSCode/Explorer/
// Finder. Renames on disk immediately on confirm (no separate "are you
// sure?" step, unlike Delete -- a rename is easy to undo by renaming back)
// and reports {old, new} via `result.file_renamed` so the caller (main.cpp)
// can retarget the file's tab if it's open in the editor.
```

- **L262:**

```
// Right-click context menu shared by file *and* folder rows: New
// File/New Folder relative to `dir` (a file's parent, so you add a
// sibling script; a folder's own path, so you add something inside it),
// plus Rename/Delete for `entry_path` itself -- either kind of entry
// supports both today. The "F2"/"Del" hints are just labels here -- the
// actual hotkeys are handled once in DrawExplorerPanel (see the comment
// there), gated on the panel having focus, so they work no matter which
// row's context menu (if any) is open.
```

- **L305:**

```
// A plain click both selects the row *and* (via TreeNodeEx's
// own default behavior) toggles it open/closed, same as
// before -- this just also records the selection so F2/Del
// and the highlight above act on folders too, not just files.
```

- **L318:**

```
// Drop target: dropping a file/folder here moves it inside
// this folder, even while it's collapsed -- same as VSCode.
```

- **L335:**

```
// ImGuiSelectableFlags_AllowDoubleClick makes Selectable()
// return true on both single and double clicks -- the
// IsMouseDoubleClicked() check below is what actually gates
// opening the file, so a single click only selects/highlights
// the row (VSCode-style) instead of immediately swapping the
// active editor tab out from under you. That selection is
// also what F2/Del act on (see the hotkey handling in
// DrawExplorerPanel), so a click always updates it even when
// it lands on the double-click that opens the file too.
```

- **L351:**

```
// Right-click also selects the row, same as left-click --
// otherwise "Rename"/"Delete" from the context menu could act
// on whatever was selected before, not the row you just
// right-clicked.
```

- **L364:**

```
// Drop target: dropping something onto a file moves it next
// to that file, i.e. into the file's own containing folder.
```

- **L384:**

```
// "###explorer" keeps the panel's real window ID stable across
// locales -- ImGui only hashes the part after "###" into the ID, so
// this stays "Explorer###explorer" in the docking layout / panel_open
// map regardless of what Tr() returns -- see builtin_panels.h.
```

- **L394:**

```
// Empty space below the tree (not any file/folder row) fills the rest
// of the panel: right-click there creates at the listing's root
// instead of inside whatever the last node happened to be, and
// dragging a file/folder there moves it back up to the project root
// -- both need a real item to hang off of, hence the Dummy.
```

- **L418:**

```
// F2 (rename) / Del (delete) act on whatever row is currently
// selected -- file or folder, same as Explorer/VSCode -- checked once
// here rather than per-row so they fire from anywhere in the panel,
// not just while hovering the selected row itself. Gated on the
// Explorer window having focus so these don't fire while e.g. the
// Code Editor panel has focus and the user is just typing/deleting
// text there; that focus also naturally goes away on its own while a
// popup (rename, delete confirm, ...) is open, so this can't
// re-trigger itself. exists() (rather than just "selected_path is
// non-empty") guards against a stale selection surviving the entry
// being deleted or renamed out from under it by some other means.
```


---

## `panels/explorer_panel.h`

- **L12:**

```
// Currently selected row (single click, or right-click for the
// context menu) -- empty means nothing selected. This is what F2/Del
// act on, and what the row-highlight in the tree reflects. Can be
// either a file or a folder path.
```

- **L20:**

```
// File the user double-clicked -- caller opens it in the Editor panel.
// Single-click only selects the row (VSCode-style: a single click on
// a file in the tree doesn't steal focus into a new/existing tab).
```

- **L24:**

```
// Path the user deleted via the right-click "Delete" confirmation (or
// the Del hotkey) -- a file or a whole folder (with its contents).
// Caller should close any open tab(s) under this path, since the
// file(s) backing them no longer exist on disk.
```

- **L29:**

```
// {old_path, new_path} when the user renamed a file/folder via the
// right-click "Rename" menu item (or the F2 hotkey), or moved one by
// dragging it onto another folder (or onto empty space, for the
// project root) in the tree. Caller should retarget any open tab(s)
// (EditorTab::file_path) under old_path so they keep tracking the
// same file instead of the stale path.
```

- **L36:**

```
// Path the user picked via the right-click "Open in Explorer" menu
// item -- a file or a folder. Caller (main.cpp, which owns the
// platform-specific window/shell code) reveals it in the OS file
// manager via studio::titlebar::RevealInFileExplorer.
```

- **L43:**

```
// Draws the Explorer panel (left dock). `p_open`: same convention as
// ImGui::Begin's own p_open -- pass the address of this panel's runtime
// visibility flag (see main.cpp's `panel_open` map) so the tab gets a
// close ("x") button and clicking it flips the flag to false, the same
// way the View menu's checkbox does. nullptr (the default) draws the
// panel with no close button, same as before this parameter existed.
```


---

## `panels/logs_panel.h`

- **L7:**

```
// UI-only state for the Logs panel. Mirrors TerminalState's selection
// fields (see panels/terminal_panel.h) but there's no input box and no
// click-to-file-position here -- general log lines don't carry a source
// position the way a script's compile errors do.
```

- **L16:**

```
// Draws the Output panel (bottom dock) as a plain, append-only log of
// everything that isn't a script's own run output: plugin load/init
// messages, PluginHost edit-apply results, etc. -- see LogBridge's
// header comment for why this is a separate stream from the Terminal's
// console. Visually the same list/copy/clear idiom as the Terminal, just
// without the run-input box or error-click navigation.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
```


---

## `panels/pending_edits_panel.cpp`

- **L27:**

```
// std::getline drops the trailing '\n' that separates each line but
// also silently drops the *file's* final line if it has no trailing
// newline of its own -- rebuilding line-by-line like this is fine
// either way for a review diff (a missing final blank line is not
// something the person needs highlighted).
```

- **L36:**

```
// A small LCS-based line diff -- good enough for reviewing an agent's
// proposed edit, not meant to be a general-purpose diff engine. O(n*m)
// in the number of lines on each side, so capped: past kMaxDiffCells
// total table entries, this gives up on a line-level diff and falls
// back to "everything old removed, everything new added" so a huge
// file still renders (just without line-level highlighting) instead of
// hanging the UI thread computing an LCS table.
// e.g. ~2000 lines x ~2000 lines
```

- **L121:**

```
// Not docked (no default_dock_slot -- this isn't a plugin panel, it's
// host UI) -- a floating window makes more sense for something that
// pops up rarely and needs a decision before it's dismissed, rather
// than permanently occupying a dock slot the rest of the session.
```

- **L166:**

```
// Closing the window via its titlebar X rejects every proposal still
// shown, rather than leaving them queued invisibly -- "nunca se
// aplica solo" cuts both ways: dismissing the review is a rejection,
// not a silent approval.
```


---

## `panels/pending_edits_panel.h`

- **L7:**

```
// Fase 5 (see PLAN_agente_ia_openrouter.md): the approval gate for
// AvaHostServices::apply_edit. Every proposal a plugin has queued (see
// PluginHost::PendingEdits) shows up here as a collapsible diff against
// the file's current contents, with Aplicar/Rechazar buttons -- nothing
// a plugin proposes ever reaches disk without a click here.
//
// Draws nothing (not even an empty window) when there are no pending
// edits, so it doesn't take up dock space or attention the rest of the
// time. Call once per frame, anywhere after PluginHost's plugins have
// had a chance to run this frame's tool calls.
```


---

## `panels/preview_panel.cpp`

- **L9:**

```
// PROPERTIES_EDITABLE: this only reads from PreviewNode into
// PropertiesState, and leaves PropertiesState::editable at its default
// (false) -- Properties shows this selection read-only. Write-back
// exists now (see designer_canvas.cpp's ToPropertiesState and
// properties_panel.cpp/main.cpp, Fase 3 of 08_DESIGNER_VIEW_PLAN.md),
// but only for a real IComponent backed by an actual .avaui file --
// this demo Component Tree has no source file to write into (see the
// note in engine_bridge.cpp), so it stays read-only on purpose.
```


---

## `panels/preview_panel.h`

- **L10:**

```
// Draws the Preview panel (bottom dock, per the Explorer/Designer/
// Properties/Preview sketch). Milestone 1: renders the fixed demo
// Component Tree as a clickable outline (not the real drag/drop
// Designer canvas yet -- that's the next milestone, once
// `page`/`stack`/`button` builtins let a real .ava script produce this
// tree instead of BuildDemoComponentTree()).
//
// Clicking a node returns its properties so the caller can feed the
// Properties panel -- see PROPERTIES_EDITABLE below for what's still
// read-only.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
```


---

## `panels/properties_panel.cpp`

- **L12:**

```
// Same rationale as explorer_panel.cpp/editor_panel.cpp/designer_canvas.cpp's
// TrFormat: a locale CSV is meant to be safely hand-edited by a
// translator, so it shouldn't also have to double as a valid printf
// format string.
```

- **L23:**

```
// Small "x" remove button, right-aligned in whatever column it's
// placed in -- shared by the properties and events tables below so
// removing a row looks/behaves identically in both. Returns true the
// one frame it's clicked.
```

- **L34:**

```
// One editable key/value table (properties or events -- same shape),
// with a per-row remove button and an "add new row" line underneath.
// `add_buffer` is the caller's own persistent std::string for the
// "new key" input -- kept outside this function (in DrawPropertiesPanel's
// local statics) so it survives across frames while the person is
// typing a new key, same reasoning as row.value being bound directly
// instead of copied into a temp buffer.
//
// Returns a PropertyEdit for whichever single row-level action (value
// committed, row removed, row added) happened this frame, using
// `value_kind`/`add_kind`/`remove_kind` to tag it correctly for
// properties vs. events -- the table drawing/interaction logic itself
// doesn't otherwise care which one it's rendering.
```

- **L59:**

```
// Index-based removal deferred until after the loop (erasing
// mid-iteration would invalidate `rows`' own iterators/indices
// for whatever's left of this same frame's loop).
```

- **L71:**

```
// PushID by index (not by key -- keys aren't guaranteed
// unique within a node's property/event list) so every row
// gets its own stable ImGui widget identity for the frames
// it exists across.
```

- **L77:**

```
// Bound directly to row.value -- this is what makes the
// table itself reflect keystrokes immediately, no extra
// "editing buffer" struct to keep in sync. What's NOT
// committed to the real IComponent until deactivation
// below is only the *source-of-truth* copy living in some
// EditorTab's DesignDocument -- see PropertyEdit's comment
// in properties_panel.h.
```

- **L85:**

```
// Deactivated-after-edit (unfocus or Enter), not every
// keystroke -- matches how every other text-entry point in
// this codebase commits, and avoids re-parsing/re-dirtying
// the document on every single character typed.
```

- **L101:**

```
// Mirror the removal locally too, so the table doesn't
// still show the just-deleted row for one extra frame
// while main.cpp's write-back (which patches the *real*
// IComponent, not this PropertiesState's own copy) catches
// up on its own next call -- same "why mirror this here"
// reasoning as tab.design.dirty/tab.dirty in main.cpp.
```

- **L110:**

```
// "Add new row" line -- own row, not inside the loop above, so
// it never gets an index that collides with a real row's ID.
```

- **L121:**

```
// Disabled with an empty key (nothing to add) or a key that
// already exists (silently skipping a duplicate is friendlier
// than adding a second, indistinguishable row with the same
// key -- the table has no way to tell them apart afterward).
```

- **L149:**

```
// Persist the two "add new key" text buffers across frames --
// static is fine here (single Properties panel, one selection at a
// time) rather than threading them through PropertiesState itself,
// which every OTHER caller (Preview's read-only path) would then
// have to carry around for no reason. Whatever's mid-typed here is
// harmless leftover text if the selection changes mid-edit -- worst
// case an add click after switching selection adds to the newly
// selected node instead, same "no destructive default" spirit as
// the rest of this fase.
```

- **L171:**

```
// Type as an editable combo, seeded from the same catalog the
// Toolbox drags from -- picking a different type only changes
// IComponent::typeName; it deliberately does NOT re-seed/merge
// default_properties for the new type (that could silently
// discard hand-edited values), so existing properties/events
// just carry over untouched even if some no longer apply to
// the new type. Simple and predictable beats clever here.
```

- **L207:**

```
// Either a Preview-panel selection (no source file to write
// into at all) or a synthetic Designer selection (a resolved
// `Componente()` import copy, see designer_canvas.h): either
// way there's nowhere real to write an edit back into, so the
// tables below stay read-only and this says so.
```

- **L238:**

```
// Events -- shown for a Designer selection only (Preview's demo
// tree never populated PropertiesState::events, see the header
// comment; an empty read-only table here would just be noise for
// it). Same editable/read-only split as Properties above.
```


---

## `panels/properties_panel.h`

- **L20:**

```
// IComponent events mirror (key = event name e.g. "on_click",
// value = handler function name) -- only populated for a Designer
// canvas selection (see designer_canvas.cpp's ToPropertiesState).
// Preview-panel selections leave this empty, same as `properties`
// conceptually always existed there but events never did.
```

- **L27:**

```
// Below: only meaningful when a Designer canvas selection populated
// this state (see designer_canvas.cpp's ToPropertiesState) -- the
// Preview panel's read-only demo tree (preview_panel.cpp) leaves
// these at their defaults, which keeps `editable` false there and
// the table read-only, same as before this fase.
```

- **L33:**

```
// False for every Preview-panel selection, and also false for a
// Designer canvas selection that landed on a *synthetic* node (a
// resolved `Componente()` import copy, see designer_canvas.h) --
// there's no real IComponent in any doc tree to write back into for
// those. True only for a selection on a real node of the active
// .avaui's own tree.
```

- **L41:**

```
// Identifies which node to patch on write-back: `source_tab_id`
// matches EditorTab::id (stable across tab reordering/renaming,
// unlike a vector index) and `selected_node_id` matches
// IComponent::NodeId() within that tab's DesignDocument::tree.
// Both stay at their defaults (-1 / empty) for a non-editable
// selection.
```

- **L51:**

```
// What a PropertyEdit represents -- previously (Fase 3) there was only
// ever one kind of edit (a property's value), so PropertyEdit didn't
// need to say which; now (9.9/9.12's leftover items -- id, type,
// add/remove properties, events) there are several, so every
// PropertyEdit says which one it is and main.cpp's write-back switches
// on it.
```

- **L58:**

```
// an existing property's value changed -- key + new_value
// IComponent "id" property changed -- new_value holds the new id, key unused
// IComponent type changed -- new_value holds the new type string, key unused
// a new property row -- key = its key, new_value = its (usually empty) initial value
// remove the property with this key -- key set, new_value unused
// an event's handler changed or a new event row was added -- key = event name, new_value = handler
// remove the event with this key -- key set, new_value unused
```

- **L67:**

```
// One committed property edit, returned by DrawPropertiesPanel when the
// person finishes editing a value (see ImGui::IsItemDeactivatedAfterEdit
// in properties_panel.cpp -- committed on unfocus/Enter, not keystroke by
// keystroke) or clicks an add/remove button. The caller (main.cpp) is the
// one that actually knows where every open DesignDocument lives, so it
// looks up `source_tab_id` / `node_id` itself and patches the real
// IComponent according to `kind` -- this struct is just the "what
// changed" message, not a mutation applied by this panel.
```

- **L83:**

```
// Draws the Properties panel (right dock). Read-only unless
// `state.editable` is set (only true for a real, non-synthetic Designer
// canvas selection -- see PropertiesState::editable above): each row's
// value becomes an editable text field, bound directly to
// `state.properties[i].value` (via imgui_stdlib's std::string overload)
// so the table itself always shows what's being typed, plus (when
// editable) an editable Id field, a Type combo seeded from
// design::GetComponentCatalog(), a remove button per property/event
// row, and an "add" row at the bottom of each table. Returns a
// PropertyEdit once any of that is committed (nullopt on every frame
// nothing was just committed) so the caller can write it back into the
// real IComponent and mark the document dirty.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
```


---

## `panels/settings_panel.cpp`

- **L15:**

```
// Which sidebar section is active -- a plain index scheme rather than a
// string compare per frame: -1 == "General" (the one fixed item),
// >= 0 == index into this frame's `settings_panels` vector. static
// (persists across frames, one Settings panel at a time) same pattern
// as open_properties_modal/open_plugins_modal used to in
// titlebar_panel.cpp before this fase.
```

- **L31:**

```
// "General" section content -- the modules_path field + Browse/Save
// button, moved as-is (same buffer/round-trip mechanics) from the old
// Properties modal (titlebar_panel.cpp, pre-Fase-2/3).
```

- **L36:**

```
// Persistent across frames, same reasoning modules_path_buf had in
// titlebar_panel.cpp: this panel is always visible (not a modal
// that reseeds on every open), so the buffer is seeded once here
// and otherwise only touched by typing or a Browse round-trip.
```

- **L52:**

```
// Language combo -- applies immediately on selection (unlike the
// modules_path field below, a dropdown pick is already a single
// deliberate action, so it doesn't need its own Save button). Only
// "en"/"es" today (see util::Locale) -- more locales are a later
// phase per PLAN_AVASTUDIO_IDE.md section 6.4.
```

- **L60:**

```
// Deliberately NOT run through Tr(): these are language endonyms
// (a language's own name for itself), not chrome -- "English" and
// "Español" are meant to read correctly in their own script
// regardless of which locale is currently active, same convention
// every OS/app language picker follows, so a Spanish-reading person
// still sees "English" (not a Spanish translation of that word) in
// this list.
```

- **L109:**

```
// No modal, so a field this "loud" (writes to disk) keeps its own
// explicit Save button rather than applying on every keystroke --
// see settings_panel.h's out_settings_dirty comment / the plan's
// reasoning for keeping this one field's existing button pattern.
```

- **L128:**

```
// Left: sidebar, fixed width, bordered -- same child-window idiom
// used elsewhere in this codebase for a scrollable side list (see
// the Plugins modal's "##PluginsList" child in titlebar_panel.cpp).
```

- **L162:**

```
// Right: content for whichever section is selected. Out-of-range
// index (e.g. the previously-selected plugin panel's owner got
// disabled/unloaded between frames) falls back to General instead
// of drawing nothing -- same "no destructive default" spirit as
// the rest of this codebase's stale-selection handling.
```

- **L175:**

```
// No Begin/End of our own here -- we're already inside
// Settings' own window/child, see settings_panel.h's
// comment on why this differs from the normal plugin-panel
// draw loop in main.cpp.
```


---

## `panels/settings_panel.h`

- **L11:**

```
// The "Settings" panel (see PLAN_settings_panel.md, Fase 2) -- a normal
// dockable tab, same family as Explorer/Properties/Preview/Terminal/
// Output (see builtin_panels.h), that internally lays itself out as a
// VSCode-style master/detail split: a left sidebar listing sections
// grouped by header ("General", "Plugins") and a right content area
// showing whatever section is currently selected. This is a manual
// ImGui::BeginChild split *inside* this one tab -- it has nothing to do
// with the outer ImGui dockspace/tabs main.cpp otherwise manages.
//
// `settings`: the same StudioSettings instance main.cpp already owns
// and persists (previously fed only into the Properties modal, see
// titlebar_panel.cpp) -- mutated in place as fields are edited.
//
// `settings_panels`: this frame's plugin_host.SettingsPanels() -- every
// RegisteredPanel a loaded plugin registered with is_settings == true
// (see plugin_api.h/plugin_host.h Fase 1). One "Plugins" sidebar item
// per entry, drawn via panel.draw(ctx, panel.user_data) in the content
// area when selected -- same AvaPanelContext mechanism main.cpp's own
// panel-drawing loop already uses for normal plugin tabs (see
// plugin_ui_bridge.h), just without an ImGui::Begin/End of its own
// since we're already inside this panel's window.
//
// `out_settings_dirty`: set to true the one frame the "General" section's
// modules_path field is actually saved (Save button clicked) -- mirrors
// TitleBarResult::modules_save_requested's role in the old Properties
// modal. main.cpp is what actually calls studio::SaveSettings() when
// this comes back true; this function only mutates `settings` in
// memory and flags that a persist is due.
//
// `out_browse_requested`: set to true the one frame the modules_path
// row's "Browse..." button is clicked -- same role as
// TitleBarResult::modules_browse_requested (main.cpp owns the native
// folder-picker dialog; this panel has no OS integration of its own).
//
// `browsed_folder`: normally "". The one frame after the person picks a
// folder from that native dialog, main.cpp passes the chosen path back
// in here so this panel can drop it into its own modules_path text
// buffer -- same round-trip DrawTitleBar's own `browsed_folder` param
// already does for the (now-removed) Properties modal.
//
// `p_open`: same ImGui::Begin p_open convention as every other panel
// here (see properties_panel.h's DrawPropertiesPanel) -- pass the
// address of panel_open["Settings###settings"] so the tab gets a close ("x")
// button. nullptr draws with no close button.
```


---

## `panels/syntax_highlight.cpp`

- **L27:**

```
// Keywords straight out of grammar/AvaLang.g4 (statement/control-flow
// literals used verbatim in the grammar's quoted terminals).
```

- **L37:**

```
// Builtin type constructors (core/src/builtins) -- colored as Types even
// when used as a call, e.g. int(x), since they name a type, not a verb.
```

- **L44:**

```
// Builtin free functions (core/src/builtins) -- offered in autocomplete;
// colored as Function via the generic "NAME immediately followed by (" rule.
```

- **L77:**

```
// Strings and f-strings ($"..."). Interpolation braces are not
// sub-highlighted -- the whole literal is treated as one string.
```

- **L138:**

```
// Everything else (operators, punctuation, whitespace, newlines):
// one char at a time, default color. Whitespace doesn't reset
// prev_keyword (so `func` / `class` still applies across the gap
// to the NAME that follows); any other punctuation does.
```


---

## `panels/syntax_highlight.h`

- **L8:**

```
// Hand-written, best-effort lexer for AvaLang (mirrors grammar/AvaLang.g4).
// It is not a full parser -- it exists purely to classify spans of source
// text for the editor's syntax-highlighting overlay and for autocomplete
// candidate collection, the same way a TextMate/VSCode grammar would.
```

- **L37:**

```
// Every identifier (NAME) that appears anywhere in the buffer, deduped,
// excluding language keywords. Used to offer user-defined function and
// variable names in autocomplete.
```


---

## `panels/terminal_panel.cpp`

- **L14:**

```
// --- background script run (out-of-process, via ava_cli.exe) ---------
// Same shape as build_panel.cpp's StartBuild -- see ScriptRunState's
// comment in terminal_panel.h for why Run works this way.
```

- **L22:**

```
// Same "Run <path>" marker EngineBridge::RunScript() itself pushes,
// written here (synchronously, on the UI thread that's calling
// StartScriptRun) so it shows up right away instead of waiting for
// the child process to produce its first byte of output.
// NOTE (i18n-7, panel inferior agrupado): this string, and every
// other engine.AppendConsoleLine(...)/run.log message in this
// function and PollScriptRun below ("Run ...", "OK", "could not
// launch...", "script exited with an error...", "the script
// crashed..."), is deliberately left untranslated for now. They're
// authored by AvaStudio, not by the compiler/VM, so §6.1 doesn't
// put them in the same bucket as raw compiler output -- but the
// "Run <path>" marker specifically has a documented duplicate in
// engine/engine_bridge.cpp (RunScript(), same literal, see the
// comment there) that has to stay byte-for-byte in sync with this
// one; migrating just the panel-side copy to Tr() would silently
// desync the two the next time someone edits one and not the
// other. Translating this whole family belongs to a fase that
// touches engine_bridge.cpp directly (not a panel), not to Terminal's
// own i18n-N -- left as a follow-up rather than folded in here.
```

- **L129:**

```
// ava_cli_path itself couldn't be started -- not a script
// problem at all, most likely a misconfigured/missing
// ava_cli(.exe). The explanatory line is already in the
// console via AppendExternalOutput above (it's what
// StartScriptRun wrote into run.log).
```

- **L142:**

```
// Normal compile/runtime error -- ava_cli.exe already printed
// "compile error: ..." / "runtime error: ..." above via
// stderr (see runtime/avacli/src/main.cpp), so the console
// already shows the actual message. This just marks the run
// as failed; no error_line/column, ava_cli doesn't expose
// those today (see ScriptRunState's comment on the trade-off).
```

- **L152:**

```
// Neither a clean exit nor a normal script-level error --
// the child process itself died (e.g. a native access
// violation from an unsafe extern binding). AvaStudio is
// still alive to say so, which is the whole point.
```

- **L195:**

```
// Joins console lines [first, last] (inclusive, both valid indices) into
// one clipboard-ready string, one ConsoleLine per output line (an error's
// embedded '\n' source excerpt stays inside its own single entry).
```

- **L229:**

```
// A stale selection can outlive the lines it points to (Clear, or a
// run that replaces the scrollback) -- drop it rather than let a
// future frame index off the end of a now-smaller console.
```

- **L237:**

```
// Capture the row's right edge before drawing anything on it, so the
// Clear button below can be pinned flush to it regardless of how wide
// the panel currently is.
```

- **L243:**

```
// Pin "Clear" (and "Copy console" just to its left, with a bit of
// breathing room between them) to the row's right edge instead of
// floating right after the label -- lines up with every other panel's
// top-right action.
```

- **L290:**

```
// Selectable (not TextUnformatted) so each line can carry a
// selection highlight and be click/shift-click/drag-selected
// like a real console -- text still gets colored per-kind via
// the same PushStyleColor the old plain-text version used.
// TextUnformatted (not Text/"%s") because compile-error
// messages can contain embedded '\n's (source excerpt + "^"
// column caret, see core/src/frontend/frontend_antlr.cpp) --
// Selectable's label handles that the same way TextUnformatted
// did, rendering every line instead of collapsing them.
```

- **L310:**

```
// An Error line with a known source position (see
// ConsoleLine::error_line in engine_bridge.h) is
// clickable like a terminal's problem matcher: jump
// straight to the offending file/line instead of only
// selecting the text. Falls back to nothing (just
// selects) for lines with no position, e.g. an error
// predating source-line tracking.
```

- **L322:**

```
// Dragging with the button still held extends the selection
// to whatever line the mouse is over, same as a normal text
// drag-select.
```

- **L334:**

```
// Right-click anywhere in the scrollback for Copy / Copy All / Select
// All -- same idiom as a terminal, and the one place Ctrl+C-averse
// users can still get the text out.
```

- **L352:**

```
// Keyboard shortcuts, gated on the scrollback itself having focus so
// Ctrl+C/Ctrl+A here don't steal those keys from the code editor or
// anywhere else while this panel merely happens to be visible.
```

- **L365:**

```
// Auto-scroll to the bottom on new output, but only if the user was
// already at (or near) the bottom -- so scrolling up to read earlier
// output doesn't get yanked back down by the next print().
```

- **L379:**

```
// --- Console input -------------------------------------------------
// Scaffolding for a future `input()` builtin -- see the long comment
// on EngineBridge::SubmitConsoleInput() for why this can't actually
// feed a running script yet. Kept enabled (not greyed out) and fully
// wired to echo + queue, so wiring up real input() later is just
// making something *read* input_queue_, not building this UI.
```


---

## `panels/terminal_panel.h`

- **L13:**

```
// Background-thread state for one in-flight "Run" (see StartScriptRun
// below). Same shape as BuildPanelState's own worker fields
// (panels/build_panel.h) -- deliberately, since it's the same pattern:
// the script runs out-of-process via ava_cli.exe instead of calling
// EngineBridge::RunScript() directly on the UI thread, so that:
//
//  - A script that blocks on a native `extern` call (e.g. libmysql's
//    mysql_real_connect with nothing listening on the other end) can no
//    longer freeze the whole window -- AvaLang's coroutines/async
//    (builtins/builtin_async.cpp) only cover bytecode-level yields and
//    timers, they cannot hand control back mid a blocking native call
//    (see vm_extern.cpp's ffi_call), so the only way to keep the UI
//    thread free is to not run the blocking call ON the UI thread.
//  - A hard native crash inside the script (e.g. an out-of-bounds
//    pointer read through mem_peek_ptr, which is intentionally unchecked
//    -- see builtins/builtin_mem.cpp) only takes down the child
//    ava_cli.exe process, not AvaStudio itself.
//
// Trade-off, and it's a real one: EngineBridge::RunScript() (still used
// for plugins' run_project_on_main_thread, see main.cpp) gets precise
// error_line/error_column/error_source straight from
// ava_last_error_line/column/source. ava_cli.exe today only prints
// "compile error: ..." / "runtime error: ..." to stderr (see
// runtime/avacli/src/main.cpp) with no structured position -- so a run
// through this path can't click-to-jump to the offending line the way
// an in-process run's Error console lines can. The error text itself
// still comes through fine, just not the line highlight.
```

- **L47:**

```
// true once the process has exited (or failed to launch)
// DrawTerminalPanel flips this once it has folded has_result into the console/TerminalState::last_run, so it only does that once
// true if ava_cli_path itself couldn't even be started (bad path) -- distinct from exit_code, which only means anything once a process actually ran
// 0 = success, 1 = normal compile/runtime error (see ava_cli's main.cpp), anything else = the child process itself crashed
```

- **L53:**

```
// UI-only state for the Terminal panel. The actual scrollback (the
// ConsoleLine history) lives in EngineBridge, not here -- see the
// comment on EngineBridge::Console() for why (it's tied to the VM's
// print callback, which outlives any one panel draw call).
```

- **L58:**

```
// scratch buffer for the console's input box widget
// true once the user has pressed Run at least once this session
// last run's summary -- lets other UI (e.g. a future status bar) show success/failure without re-scanning the console
```

- **L62:**

```
// Selection range over console line indices, both inclusive. -1 means
// "no selection". anchor is where the click/drag started, cursor is
// where it currently ends (they can be in either order -- callers use
// std::min/max of the pair to get the actual [first, last] range).
```

- **L69:**

```
// Backing state for the interactive Run button/shortcut (main.cpp),
// polled and folded into EngineBridge::Console()/last_run above by
// DrawTerminalPanel every frame. See ScriptRunState's own comment
// for why Run works this way instead of calling
// EngineBridge::RunScript() directly.
```

- **L77:**

```
// Starts running `script_path` via `ava_cli_path` (e.g. the result of
// DetectAvaCliPath() / StudioSettings::build_ava_cli_path -- same
// resolution the Build panel already uses, see util/ava_cli_locator.h)
// on a background thread. No-op if a run is already in flight
// (state.run.running) -- caller should disable/hide the Run action
// while that's true, same convention as BuildPanelState::building. Joins
// any previous (already-finished) worker thread first, same as
// StartBuild in build_panel.cpp.
//
// Does NOT save the active tab first -- callers must ensure
// `script_path` reflects what they want to actually run (main.cpp calls
// SaveTab() on the active tab right before this, since ava_cli.exe reads
// the file from disk, unlike the old in-process RunScript() which ran
// the editor's in-memory buffer directly).
```

- **L93:**

```
// Folds ScriptRunState::run's progress into EngineBridge::Console() and,
// once the run finishes, into TerminalState::last_run/has_run_result --
// same fields EngineBridge::RunScript() itself sets, so callers that
// read them (e.g. main.cpp's plugin_callbacks.get_last_run_output)
// don't need to know whether the last run happened in-process or via
// StartScriptRun.
//
// IMPORTANT: call this once a frame UNCONDITIONALLY, regardless of
// whether the Terminal panel is currently open -- DrawTerminalPanel
// itself is only called while its tab is open (see main.cpp), so a run
// started while the panel is closed would otherwise sit finished in
// ScriptRunState forever, never folded in, and
// get_last_run_output()/has_run_result would silently go stale.
```

- **L108:**

```
// Returned by DrawTerminalPanel when the user clicks an Error line in the
// console that carries a known source position (ConsoleLine::error_line
// != 0 -- see engine_bridge.h). main.cpp opens/focuses `file_path`'s tab
// and highlights `line`/`column` on it, the same way it already does
// right after a failed Run -- this just lets an *older* error line in
// the scrollback jump there again, without re-running anything.
```

- **L121:**

```
// Draws the Terminal panel (bottom dock) as an execution console: every
// print() from the running script, interleaved with Run/error/result
// markers, accumulated across every run this session like a real
// terminal -- replaces the old static "last run result + Component Tree
// JSON" view. Only ever shows a script's own output; anything else
// (plugin lifecycle messages, host events) goes to the separate Output
// panel instead -- see panels/logs_panel.h and util/log_bridge.h.
//
// The bottom input line calls EngineBridge::SubmitConsoleInput() on
// Enter and echoes the text into the console, but nothing in the
// language reads from it yet -- there is no `input()` builtin. See
// engine_bridge.h for why and what's needed before there can be one;
// this is scaffolding for that, not a working REPL today.
//
// Returns a request when an Error line with a known source position was
// clicked this frame (see TerminalFileClickRequest above); nullopt every
// other frame. Consumed by main.cpp right after the call, same pattern
// as DrawPreviewPanel's return value.
//
// `p_open`: same convention as ImGui::Begin's own p_open -- pass the
// address of this panel's runtime visibility flag (see main.cpp's
// `panel_open` map) so the tab gets a close ("x") button that flips it
// to false, the same way the View menu's checkbox does. nullptr (the
// default) draws the panel with no close button.
```


---

## `panels/titlebar_panel.cpp`

- **L33:**

```
// Flat window-control button (minimize/maximize/close): an invisible
// button (so we control the exact hover/hit rect ourselves) plus a
// small hand-drawn glyph, since relying on font glyph coverage for
// "-", a square, or "x" is fragile across fonts and platforms -- VSCode
// draws its own caption glyphs the same way.
```

- **L118:**

```
// --- Brand icon (VSCode-style: just the icon, no text label) ----------
// Draws the real Ava Studio logo (avastudio.png, baked into the exe --
// see src/branding/logo_texture.h) instead of a flat orange dot.
// kIconSize is bigger than the old dot's 14px: a placeholder dot reads
// fine at any size, but the logo has real detail that needs the extra
// pixels to stay legible at title-bar scale. It's drawn 1:1 (the
// logo's own square aspect ratio), not stretched to any other shape.
```

- **L133:**

```
// Fallback in the unlikely event the embedded logo failed to
// decode/upload -- keeps the titlebar from showing a hole.
```

- **L141:**

```
// --- File / Run menu ----------------------------------------------
// Plain buttons that each open their own popup explicitly, instead of
// ImGui::BeginMenu() -- BeginMenu() assumes it lives inside a real
// ImGuiWindowFlags_MenuBar row, and a real menu bar would force its
// own dedicated top strip, breaking this titlebar's single-row custom
// layout (brand mark / centered file name / window buttons all share
// this same row). Used loose like before, BeginMenu()'s "close on
// click outside" logic doesn't reliably see clicks on this
// NoBringToFrontOnFocus window, so it can get stuck open. Explicit
// OpenPopup/BeginPopup doesn't have that problem.
// Three equal-size pill buttons (File / Run / About). Always-visible
// background at roughly the same visual weight in every state --
// normal state used to be near-transparent while hover was fully
// opaque, so whichever button had the mouse over it read as visibly
// "bigger"/more prominent even though the pixel dimensions were
// identical. Now normal/hover/active are all solid, just progressively
// lighter, so the three buttons look like a consistent set at rest.
```

- **L161:**

```
// Positioned explicitly from the icon's known geometry instead of
// ImGui::SameLine() chained off the previous item -- SameLine() derives
// its Y from whatever the last item's line height/baseline was, which
// is why File used to end up a pixel or two off from Run/About whenever
// the item before it (the old brand text) had different metrics. An
// absolute SetCursorPos() can't inherit that kind of drift.
```

- **L177:**

```
// Anchor the dropdown directly under the button that opened it
// (VSCode-style) instead of ImGui's default of "wherever the mouse
// happened to click" -- otherwise a click near a button's edge can
// make the popup appear noticeably off to the side.
```

- **L182:**

```
// Constraint en vez de SetNextWindowSize: 210px es el ancho de
// siempre, pero este popup es el único que tiene un BeginMenu()
// adentro (Preferences) -- ImGui necesita poder ensanchar la
// ventana si su propio cálculo de columnas (el que ubica la flecha
// de submenu a la derecha, mismo mecanismo que alinea "Ctrl+N" etc.
// en los demás items) pide más lugar. Un ancho fijo se lo impediría
// y la flecha terminaría mal posicionada -- dejarlo crecer es lo
// que hace que la flecha nativa de BeginMenu se vea bien sin tener
// que dibujar nada a mano.
```

- **L214:**

```
// Only meaningful for a .avaui tab (see ToggleTabViewMode's own
// no-op guard) -- shown unconditionally, same as Save/Close Tab
// above staying enabled with no tab open, rather than
// special-casing this one item to disable/hide.
```

- **L245:**

```
// --- View menu -------------------------------------------------------
// Same VSCode idea as the reference screenshot: one checkbox per
// panel that can be shown/hidden, checked when it's currently
// visible. Built-ins first (see panels/builtin_panels.h), then a
// separator and one entry per plugin panel -- both sections just
// read/write the same `closed_panels` list, so a single click
// handler (result.panel_toggle_requested) covers either kind; see
// its comment in the header for why.
```

- **L265:**

```
// Migrated panels (tr_key set) show the translated label;
// everything else still shows its plain fallback_label until
// its own i18n-N phase -- see the comment in builtin_panels.h.
```

- **L305:**

```
// About: its own top-level button next to File/Run (not buried
// inside a dropdown) -- opens the modal directly on click, same size
// as its siblings.
```

- **L317:**

```
// Checked after both BeginPopup/EndPopup calls above so it reflects
// this frame's actual state (a popup that just got closed by
// MenuItem()/Escape/etc. this same frame should not keep forcing the
// wider hit region on the next frame).
```

- **L324:**

```
// "##AboutModal" is the fixed ID suffix (see builtin_panels.h's
// "###id" comment for the same rule applied to popup titles): the
// visible prefix is rebuilt from Tr() every frame, but both
// OpenPopup() and BeginPopupModal() below always compute it the same
// way in the same frame, so the two calls never disagree even as the
// active locale changes.
```

- **L335:**

```
// Re-centered every frame against the *current* viewport size, not just
// when the popup opens -- using GetCenter() (as opposed to a
// one-time/cached position) is what keeps it centered whether the
// window is maximized, restored, or resized while the popup is open.
```

- **L345:**

```
// Every line in this modal is centered under the logo, VSCode/
// JetBrains "About" style -- CalcTextSize() + SetCursorPosX() since
// ImGui has no built-in centered-text widget. Window padding
// cancels out of the math (see the Close button below, which relied
// on the same fact before this redesign), so it's just
// (window width - item width) / 2.
```

- **L359:**

```
// Real Ava Studio logo (see src/branding/logo_texture.h) instead of
// no brand mark at all -- an About dialog with no logo was the
// actual gap here, not just a text-formatting issue.
```

- **L385:**

```
// Drawn as plain colored text (not a Button) so it reads as a
// hyperlink rather than a UI control, with a manual underline
// and hand cursor on hover for the usual "this is clickable"
// affordance, same idea as a browser's status-bar link hint.
```

- **L419:**

```
// --- Plugins modal -----------------------------------------------------
// A real window, not a cramped dropdown -- "File > Plugins..." opens
// this instead of a popup under a button, same family as the
// Properties modal above. Lists every .dll/.so PluginHost found in
// plugins/ (see the `plugins` param) with a checkbox each; toggling
// one sets result.plugin_toggle_requested for main.cpp to act on
// (flip StudioSettings::disabled_plugins, SaveSettings,
// PluginHost::Reload -- takes effect immediately, no restart) before
// the next PluginHost::ScanAvailable() snapshot comes back around.
// Same "##" ID-stability note as about_title above.
```

- **L457:**

```
// `enabled` already reflects the NEW state ImGui
// just applied to the widget -- main.cpp only needs
// to know *which* plugin changed, it re-derives
// on/off from settings.disabled_plugins itself.
```

- **L464:**

```
// Enabled but not currently loaded: either
// LoadAll() rejected it (bad ABI, init failed --
// see the Logs panel's log) or a Reload() triggered
// this same frame hasn't run yet.
```

- **L472:**

```
// Fase 9: display name / version / author, one muted
// line under the checkbox -- indented to visually
// belong to that plugin, not the next one in the list.
// Any piece the plugin didn't export is just left out
// rather than shown as "unknown"/empty parens.
```

- **L492:**

```
// Per-panel show/hide (previously a second list in this same
// modal) now lives in the "View" menu instead -- see its
// dropdown right next to File, which covers both built-in and
// plugin panels in one place instead of splitting them across
// two different menus.
```


---

## `panels/titlebar_panel.h`

- **L8:**

```
// defined in panels/editor_panel.h
// defined in util/settings.h
// defined in plugins/plugin_host.h
// defined in plugins/plugin_host.h
```

- **L13:**

```
// Bounding box of a drawn UI element in screen coordinates -- the same
// space as ImGui::GetItemRectMin/Max() on the main viewport. main.cpp
// feeds these into platform/win32_titlebar.h so OS-level window dragging
// (WM_NCHITTEST -> HTCAPTION) doesn't swallow clicks meant for the
// buttons instead.
```

- **L29:**

```
// File/Run menu buttons also need to be registered as real (HTCLIENT)
// hit regions with platform/win32_titlebar.h, or Windows treats
// clicking them as "drag the titlebar" and the click never reaches
// ImGui -- see the comment above UpdateHitRegions().
```

- **L37:**

```
// True while the File, View, or Run dropdown is open. Windows swallows clicks
// on the parts of the titlebar strip that aren't registered as extra
// hit regions (it treats them as HTCAPTION / "drag the window" before
// the click ever reaches ImGui), so a click meant to dismiss an open
// dropdown by clicking elsewhere on the titlebar never arrived. main.cpp
// uses this flag to temporarily open up the whole strip as a real
// (HTCLIENT) region while a dropdown is open, so ImGui's normal
// click-outside-closes-popup behavior can actually see the click.
```

- **L46:**

```
// File menu actions that need main.cpp (native dialogs / window
// close) -- Save stays as editor_state.save_requested like before,
// since it doesn't need anything outside the editor state.
```

- **L55:**

```
// "File > Preferences > Settings" (Ctrl+,). main.cpp reacts by
// ensuring panel_open["Settings###settings"] = true and focusing
// that tab, same pattern as reopening a panel toggled from View --
// see main.cpp.
```

- **L61:**

```
// "Run > Build Executable..." (Ctrl+B), see panels/build_panel.h.
// main.cpp reacts the exact same way as open_settings_requested,
// just against panel_open["Build###build"] instead.
```

- **L66:**

```
// "Plugins" menu (see the `plugins` param below). Set to the
// file_name of whichever PluginInfo checkbox the user clicked this
// frame, "" otherwise -- main.cpp is what actually flips it in
// StudioSettings::disabled_plugins, persists that, and calls
// PluginHost::Reload(), since this function has no access to
// PluginHost or the settings file itself.
```

- **L74:**

```
// Per-panel visibility toggle -- set by either the "Plugins" modal's
// panel list (see the `panels` param below) or the "View" menu's
// checkboxes, whichever the user clicked this frame ("" otherwise).
// Covers BOTH plugin panels (by RegisteredPanel::name) and built-in
// ones (by the literal names in panels/builtin_panels.h -- "Explorer",
// "Properties", etc.), since both are just entries in the same
// StudioSettings::closed_panels list and the same runtime
// `panel_open` map in main.cpp. main.cpp flips it in
// closed_panels, persists that, and syncs `panel_open` -- this
// function has no access to that map itself, same reasoning as
// plugin_toggle_requested above.
```

- **L88:**

```
// Draws Ava Studio's VSCode-style title bar: brand mark + name, the
// File/Run menu, the active file name centered (like VSCode's window
// title), and flat minimize/maximize/close buttons on the right.
// `is_maximized` swaps the maximize icon for the "restore" icon, and
// `height` is both the drawn height and the caller's cue for how much
// vertical space to reserve above the dockspace. Save/Run menu clicks
// are applied directly onto `editor_state` (same as the old in-dockspace
// menu bar did) -- only the window-chrome buttons come back through the
// return value, since only main.cpp knows how to talk to GLFW/the OS.
//
// `plugins`: the current PluginHost::ScanAvailable() snapshot, used
// only to draw the "Plugins" menu's checkboxes (name, enabled/disabled,
// currently loaded or not) -- this function never loads/unloads
// anything itself, see TitleBarResult::plugin_toggle_requested.
//
// `panels`: the current PluginHost::Panels() snapshot -- every panel a
// loaded plugin has registered, regardless of whether the user closed
// its tab. Drawn as a second checkbox list in the same "Plugins" modal
// (checked = tab currently visible) so closing a panel from its own tab
// X still has an easy way back, without hunting for which plugin owns
// it -- see TitleBarResult::panel_toggle_requested. `closed_panels` is
// StudioSettings::closed_panels, read here only to know which
// checkboxes start unchecked.
//
// The "View" menu (see kBuiltinPanelNames in panels/builtin_panels.h)
// reads that same `closed_panels` list to draw a checkbox for every
// built-in panel (Explorer, Properties, Preview, Terminal, Output),
// followed by a checkbox for every plugin panel in `panels` -- checked
// when currently visible, unchecked when closed. Both sections funnel
// into the same TitleBarResult::panel_toggle_requested field. Each
// built-in entry's label is Tr()'d once that panel has been migrated
// (BuiltinPanelInfo::tr_key set) and shown as-is otherwise -- see the
// struct's own comment for why only some entries have it so far.
```


---

## `panels/toolbox_panel.cpp`

- **L14:**

```
// One row: an icon-less label plus a small "container" hint tag for
// entries that accept children (Column/Row/Stack/Grid/Flex/Page) --
// mirrors the Explorer's folder-vs-file visual distinction so it's
// obvious at a glance what you can drop *into* once it's on the
// canvas, without needing a real icon set yet.
```

- **L22:**

```
// The whole row is the drag source -- payload is just the type
// string, the designer canvas is the one that knows how to turn
// that into a real IComponent (via design::AddComponentNode) on drop.
```

- **L46:**

```
// Catalog is already sorted by ComponentTypeInfo::order and carries
// its own ComponentTypeInfo::category (both come from
// data/design/component_catalog.csv, see B.6 in PLAN_UNIFICADO_AVAUI.md) --
// this just draws a new header each time the category changes.
```


---

## `panels/toolbox_panel.h`

- **L5:**

```
// ImGui payload type shared between the drag source here and the drop
// targets in designer_canvas.cpp -- payload data is the component
// type string (e.g. "button"), NUL-terminated, exactly as
// ComponentTypeInfo::type stores it. Kept as one named constant so the
// two panels can't drift (a typo in either string would silently break
// drag&drop with no compile error).
```

- **L13:**

```
// Draws the Toolbox panel (own ImGui::Begin/End, same self-contained
// style as DrawPreviewPanel/DrawPropertiesPanel): one row per entry in
// design::GetComponentCatalog(), each a BeginDragDropSource carrying
// its `type` string under kToolboxDragDropId.
//
// Per 08_DESIGNER_VIEW_PLAN.md section 5.5, this is only meaningful
// docked next to Explorer while a .avaui tab in Design view is active
// -- that visibility wiring is Fase 1/main.cpp's job (not done yet),
// so for now this always draws when called. It has no dependency on
// EditorTab/view_mode itself, so it's safe to call unconditionally
// once that wiring exists.
```


---

## `platform/win32_titlebar.cpp`

- **L129:**

```
// NOTE: we deliberately do NOT call DwmExtendFrameIntoClientArea here.
// It's the usual way to keep the native drop shadow on a borderless
// window, but GLFW/WGL windows use the legacy (BitBlt-model) OpenGL
// swap chain, not a DXGI flip-model swap chain -- and DWM only
// composites the extended margin correctly for flip-model/Direct3D
// content. On a BitBlt-model GL window it instead paints the current
// Windows accent color solid over the extended area (that's the
// orange full-width bar bug). We lose the native drop shadow as a
// result; if that's wanted later, draw a fake shadow ourselves with
// ImGui/a few translucent rects instead of via DWM.
```

- **L168:**

```
// SW_SHOWNORMAL launches whatever the user has set as their default
// browser, same as double-clicking a link anywhere else in Windows.
```

- **L211:**

```
// Modern Common Item Dialog (the same file-browser look "Open File..."
// uses -- address bar, sidebar, list/details view) restricted to
// folders via FOS_PICKFOLDERS, instead of the old-style SHBrowseForFolder
// tree control, which looks dated and feels like a different app.
```

- **L275:**

```
// A file: open its *parent* folder with the file itself
// highlighted, same as VSCode/Windows' own "Open Containing
// Folder" / "Show in Explorer". /select, needs the whole thing as
// one quoted parameter string, not a bare path.
```


---

## `platform/win32_titlebar.h`

- **L5:**

```
// Custom Win32 title bar: removes the OS-native caption/border (the grey
// Windows title bar) while keeping native move/resize/Aero-Snap/minimize/
// maximize behavior, so Ava Studio can draw its own VSCode-style dark
// title bar with ImGui instead. Windows-only -- on other platforms these
// calls are no-ops and GLFW's normal decorated window (with its native
// title bar) is used unchanged.
//
// How it works (the same trick used by Windows Terminal and other modern
// Win32 apps with a custom frame):
//   1. WM_NCCALCSIZE returns an empty non-client area, so the whole
//      window becomes "client area" -- no OS title bar or border gets
//      painted.
//   2. WM_NCHITTEST is answered manually so Windows still treats the
//      strip we draw our own title bar in as HTCAPTION (drag to move,
//      double-click to maximize, right-click for the system menu,
//      drag-to-edge for Aero Snap) and the outermost few pixels as
//      resize handles (HTLEFT/HTRIGHT/HTTOP/...), while the area over
//      our own minimize/maximize/close buttons stays HTCLIENT so ImGui
//      receives the click normally instead of it being swallowed as a
//      caption click.
//   3. DwmExtendFrameIntoClientArea keeps the native drop shadow (and,
//      on Windows 11, rounded corners) even though there's no
//      non-client frame left to carry it.
```

- **L36:**

```
// A clickable region in screen coordinates -- the same space as
// ImGui::GetItemRectMin/Max() on the main viewport, which is what the
// title bar panel hands back.
```

- **L43:**

```
// Call every frame after drawing the custom title bar, so WM_NCHITTEST
// knows (a) how tall the draggable caption strip is, and (b) where all
// the real, clickable widgets living inside that strip are (minimize/
// maximize/close plus anything else, like the File/Run menu buttons),
// so clicks on them reach ImGui instead of being swallowed as a caption
// drag. `extra_rects`/`extra_count` covers any additional buttons beyond
// the three window-chrome ones -- every clickable widget drawn inside
// the titlebar height must be listed here or Windows will treat clicks
// on it as "drag the window" instead of forwarding them.
```

- **L55:**

```
// True while the window is maximized -- lets the title bar draw the
// "restore" icon (two overlapping squares) instead of the maximize icon
// (one square), matching VSCode/Windows conventions.
```

- **L60:**

```
// Opens `url` in the OS default browser (e.g. from the About dialog's
// GitHub link). No-op on platforms where this isn't implemented.
```

- **L64:**

```
// Native "Open"/"Save As" file dialogs, filtered to .ava scripts (with an
// "all files" fallback). Return false if the user cancelled.
```

- **L69:**

```
// Native "Open Folder" picker (VSCode-style: pick a directory to use as
// the project's working directory / Explorer root), rather than a single
// file. Uses the modern Windows Common Item Dialog restricted to folders,
// the same look as OpenFileDialog above. Returns false if the user
// cancelled.
```

- **L76:**

```
// Reveals `path` in the OS file manager (Windows Explorer): opens its
// containing folder with the item itself pre-selected/highlighted, same
// as VSCode's "Reveal in File Explorer" / "Open Containing Folder". If
// `path` is itself a folder, opens that folder's own contents instead of
// selecting it inside its parent. No-op on platforms where this isn't
// implemented (see file header comment).
```


---

## `plugins/plugin_api.h`

- **L4:**

```
// Ava Studio Plugin ABI -- Fase 0.
//
// Dear ImGui has no stable ABI between compilations: a .dll/.so built
// separately from ava_studio.exe cannot call ImGui::Begin() safely
// unless it was built with the exact same compiler/flags/ImGui commit
// as the host -- any later recompile of Ava Studio would silently break
// every third-party plugin. So a plugin never links ImGui (or any C++
// Ava Studio header) directly. It talks to the host through this file
// only: a plain C struct of function pointers, versioned, the same
// pattern AvaHost already uses for its own Stable C API (see
// runtime/avalang/api/include/avalang.h) and the same spirit as
// avaui's IRenderer (DrawRectangle/DrawText/DrawButton) -- a small,
// closed set of drawing primitives instead of raw ImGui access.
//
// A plugin is a shared library exporting exactly these three C
// symbols (see the *_SYMBOL names below):
//   int  ava_plugin_abi_version(void);
//   bool ava_plugin_init(AvaStudioHost* host);
//   void ava_plugin_shutdown(void);
//
// ava_plugin_abi_version() is checked BEFORE the host ever calls
// ava_plugin_init() with a live AvaStudioHost* -- a plugin built
// against an older/newer ABI gets rejected outright instead of being
// handed a struct whose layout it might misinterpret (fields only ever
// get appended at the end in a new AVA_STUDIO_PLUGIN_ABI_VERSION, never
// reordered or removed -- see the note above the struct below).
//
// Everything here is plain C (no C++ name mangling, no STL types
// crossing the boundary) so it works from any language/compiler that
// can produce a native shared library, not just the same MSVC version
// Ava Studio itself is built with.
```

- **L43:**

```
// Bump this whenever AvaStudioHost's layout changes. New fields are
// only ever appended at the end of AvaStudioHost / AvaUiApi /
// AvaHostServices -- never inserted in the middle, reordered, or
// removed -- so a plugin built against ABI N can still be loaded (and
// simply not use) the fields a newer host added in ABI N+1. The host
// itself still only *offers* to load a plugin whose reported version
// is <= its own; see plugin_host.cpp.
// Fase 5 (see PLAN_agente_ia_openrouter.md) appended AvaHostServices::
// apply_edit/run_project at the end of the struct -- existing fields
// keep the same offsets, so ABI 1 plugins still load fine (see the
// note above), they just don't see these two.
// Fase 6 appended AvaHostServices::design_add_component/
// design_edit_component -- same rule, ABI 1/2 plugins still load,
// they just don't see these.
// Fase 8 appended AvaUiApi::input_text_multiline_submit -- same rule,
// ABI 1/2/3 plugins still load, they just don't see it (and keep using
// plain input_text_multiline, where Enter always inserts a newline).
// Fase 9 appended AvaUiApi::input_text_multiline_submit_hint and
// button_disabled -- same rule again.
// Fase 6 of PLAN_settings_panel.md appended AvaPanelRegistration::
// is_settings -- ABI 1..5 plugins still load fine, they just always
// get is_settings == false (the struct is zero-initialized on the
// plugin's side via `AvaPanelRegistration registration{};`, the
// pattern ai_agent_plugin.cpp already uses, so this comes out false
// by default with no plugin-side change needed).
// Fase 10 of PLAN_agente_ia_openrouter.md appended AvaUiApi::
// text_colored/selectable_message at the end -- same rule, ABI 1..6
// plugins still load fine, they just don't see them (and keep
// rendering chat history as plain, unselectable text_wrapped() lines,
// same as before).
```

- **L77:**

```
// Opaque per-panel drawing context. A plugin never looks inside this --
// it only ever passes the pointer it was handed straight back into the
// AvaUiApi calls. Host-side, this identifies which ImGui window
// (panel) the primitive should draw into this frame.
```

- **L83:**

```
// Where the host tries to dock a newly-registered panel the very
// first time Ava Studio runs (mirrors the existing
// ImGui::DockBuilderDockWindow calls in main.cpp). Purely a hint for
// that first layout -- like every other panel, the user can drag it
// anywhere afterward and ImGui remembers that for next time.
```

- **L95:**

```
// Called once per frame while the panel is visible/docked, same as any
// built-in panel's DrawXPanel(). `user_data` is whatever the plugin
// passed to register_panel() -- typically a pointer to the plugin's
// own state struct (chat history, input buffer, etc.), since the
// plugin has no other hook into "per-frame" than this callback.
```

- **L102:**

```
// --- UI primitives ----------------------------------------------------
// Minimal set to build a usable panel (a chat view is the driving
// case -- see PLAN_agente_ia_openrouter.md Fase 1): text, a button,
// single/multi-line text input, a combo box, and a scrollable child
// region to hold a growing list of messages. Extend this list later
// (Fase 7+) rather than letting plugins reach for ImGui directly --
// that's the whole point of the boundary.
```

- **L112:**

```
// Wraps at the panel's width, unlike label() -- use for anything
// longer than one short line (a chat message body, an error
// message, etc).
```

- **L119:**

```
// Same in/out contract as ImGui::InputText: `buffer` is the
// plugin's own storage (a member of its state struct), `buffer_size`
// is its capacity in bytes. Returns true the frame the text
// changed. `label` is also the widget's ImGui id -- must be unique
// within this panel (append "##something" for a blank visible
// label, exactly like real ImGui).
```

- **L132:**

```
// `items`/`items_count`: a plain array of C strings, not a single
// "a\0b\0c\0" blob -- simpler to build from a plugin's own
// std::vector<std::string> (collect .c_str()s into a temporary
// array before calling). `current_index` is read on entry and
// written back on change; returns true the frame the selection
// changed.
```

- **L142:**

```
// Places the next widget on the same line as the previous one --
// EXCEPT the host may still drop it to a new line anyway if the
// panel isn't wide enough for it (a narrow docked panel is common:
// Explorer/Editor/Output all fight for width too). This is
// deliberate: an input_text()+same_line()+button() row is a
// completely normal thing to write, and the alternative (the host
// honoring same_line() unconditionally) is a button or field
// silently clipped past the panel's edge with no way back to it
// short of a horizontal scrollbar -- worse for reading text or
// using a form either way. A plugin never needs to compute panel
// width itself to avoid this; just call same_line() normally.
```

- **L156:**

```
// Scrollable child region -- `id` must be unique within the panel.
// begin_child returns false if the region is clipped/collapsed
// (mirrors ImGui::BeginChild); a plugin should still call
// end_child() unconditionally either way, exactly like real ImGui.
```

- **L163:**

```
// Scrolls the *current* child region (call between begin_child/
// end_child) to its bottom -- the "stick to latest message while
// streaming" behavior a chat panel needs every frame new text
// arrives.
```

- **L169:**

```
// Fase 8: same widget as input_text_multiline, but wired for chat-
// style submission -- plain Enter submits (reports it through
// `out_submit` and does NOT insert a newline), Shift+Enter inserts
// a newline like any other character. `out_submit` may be NULL if
// the caller doesn't care (equivalent to plain input_text_multiline
// plus swallowing bare Enter). Returns true the frame the buffer's
// text changed, exactly like input_text_multiline -- check
// `*out_submit` separately to know whether that change was "typed a
// character" or "hit Enter to send".
```

- **L181:**

```
// Fase 9: same as input_text_multiline_submit, but shows `hint` as
// greyed-out placeholder text whenever the buffer is empty (like
// ImGui::InputTextWithHint) -- so an empty chat box reads e.g.
// "Describe qué construir" instead of sitting there blank. Pass ""
// (or NULL) for no hint, which behaves exactly like
// input_text_multiline_submit above.
```

- **L190:**

```
// Same as button(), but visually greyed out and unclickable while
// `disabled` is true -- always returns false in that case. For a
// send button that should only "light up" once the user has typed
// something.
```

- **L196:**

```
// Pixel height of one line of text at the current font/size --
// lets a plugin size a growing multiline input (N lines typed ->
// N * text_line_height() + padding) without needing raw ImGui
// access just for that one measurement.
```

- **L202:**

```
// Fase 10: same as text_wrapped, but in an arbitrary RGBA color
// (0..1 per channel) instead of the panel's default text color --
// lets a plugin tag things like a chat role label ("Tú"/"Agente")
// or a dim system line without needing a full styled widget just
// for that.
```

- **L209:**

```
// Fase 10: a chat message bubble. Two problems this solves that
// text_wrapped() can't:
//  1. text_wrapped() renders plain ImGui text, which has no
//     selection support at all -- there is no way to drag-select or
//     Ctrl+C anything out of it. This renders the same text as a
//     read-only text field instead (real mouse-drag selection and
//     copy, exactly like any other text box), while still being
//     un-editable -- the point is to let the user copy their own
//     chat history, not to let them rewrite it.
//  2. It auto-sizes its height to the wrapped content at the
//     current panel width, the same way a chat bubble grows to fit
//     its message -- the caller doesn't pre-compute a line count or
//     pixel height the way input_text_multiline's caller has to.
// `text_color`/`bg_color` (RGBA 0..1) tint the message text and its
// background respectively, so two roles (e.g. user vs. assistant)
// read as visually distinct bubbles instead of both being plain
// text behind a "Tú:"/"Agente:" prefix. `id` must be unique within
// the panel, same rule as begin_child's `id`.
```

- **L232:**

```
// --- Read-only host services (Fase 0) ----------------------------------
// Fase 3 (automatic context) and Fase 4 (read-only tool calling) are
// both just the ai_agent plugin calling these same three getters --
// nothing new needs to be added to the host for them, which is the
// point of exposing them here from the start rather than only once a
// consumer needs them.
```

- **L239:**

```
// Absolute path to the currently open project's root folder (the
// same folder Explorer is rooted at -- see ExplorerState::root_dir).
// Never null; returns an empty string if nothing meaningful is
// open yet. Host-owned, valid only until the next host call --
// copy it (e.g. into a std::string) if you need it past this frame.
```

- **L246:**

```
// The active editor tab, if any. Returns false (and leaves every
// out-param untouched) when no tab is open, or the active tab is
// the startup Welcome tab. `out_path` is "" for an unsaved/untitled
// buffer. `out_selection_start`/`out_selection_end` are UTF-8 byte
// offsets into `out_content`, or both -1 if the host build in use
// doesn't report a selection yet (true today -- selection
// reporting lands with Fase 3, this signature is already shaped
// for it so plugins don't need updating when it does). All
// pointers are host-owned, valid only for this frame.
```

- **L258:**

```
// The most recent Run/compile result this session (see
// panels/terminal_panel.h / EngineBridge::RunScript). Returns false
// if nothing has been run yet. `out_text` is the same summary
// string shown in the Output panel ("OK -> ..." or the error
// text); host-owned, valid only for this frame.
```

- **L265:**

```
// Prints one line into Ava Studio's own Output panel, prefixed
// with the plugin's registered name -- lets a plugin surface
// diagnostics or results the same place a failed compile already
// does, instead of needing its own separate log surface.
```

- **L271:**

```
// --- Write services (Fase 5) ---------------------------------------
// Both of these can be called from any thread the plugin owns (not
// just the frame that draws its panel) -- the ai_agent plugin's
// tool-use loop, for example, runs on its own worker thread (see
// ai_agent_plugin.cpp's SendMessage). The host marshals the actual
// work back onto its main thread internally; a plugin never needs
// to know or care which thread it called these from.
```

- **L279:**

```
// Proposes replacing the *entire* contents of `path` (relative to
// the open project's root, same rules as read_file: no `../`, no
// absolute override) with `new_content`. Never writes anything by
// itself -- queues the proposal for the person to review (the host
// shows a diff against the file's current contents, with Aplicar/
// Rechazar buttons) and returns immediately, before it's been
// decided either way. `description` is a short human-readable
// summary of the change shown above the diff (e.g. "Arreglar el
// typo en el mensaje de error"); pass "" if there's nothing worth
// saying beyond the diff itself.
//
// Returns true once the proposal is queued (this says nothing
// about whether the person will approve it -- a plugin has no way
// to find that out today, Fase 5 is fire-and-forget from here).
// Returns false if `path` doesn't resolve inside the project root,
// no project is open, or `path`/`new_content` is null -- in which
// case `*out_error` (if non-null) is set to a host-owned message
// describing why, valid only until the next call into this host on
// the calling thread.
```

- **L301:**

```
// Runs the exact same compile+run pipeline as pressing F5 on the
// currently active editor tab, and blocks the calling thread until
// it actually finishes -- unlike apply_edit, there is nothing to
// approve here (running code the person can already see and Run
// themselves isn't a new capability the way writing a file is), so
// this can just do the work and hand back a real result.
//
// Returns true with `*out_output`/`*out_had_error` filled in
// (host-owned, valid only until the next call into this host on
// the calling thread -- copy what you need before making another
// one) once the run has completed. Returns false if there was no
// real tab open to run (no tabs, or only the Welcome tab) --
// `*out_error` (if non-null) explains why.
```

- **L316:**

```
// --- Design services (Fase 6) ---------------------------------------
// Both work against the ACTIVE editor tab's .avaui document (see
// AvaHostServices::get_active_file for the general "active tab"
// notion) and, like apply_edit, never write anything by
// themselves: they compute the resulting AvaLang UI source and
// queue it as a normal apply_edit-style proposal for the person to
// review (Aplicar/Rechazar) against the tab's file path. This is
// deliberately the SAME approval gate as apply_edit, not a second
// one -- "el agente nunca escribe ni ejecuta nada sin confirmacion
// explicita del usuario" (see the plan's principio rector) has to
// hold here exactly as it does for a plain text edit. Returns
// false if the active tab isn't a .avaui document, or (design_add_
// component only) if `parent_id`/`type` don't resolve to a real
// container / known catalog type -- `*out_error` (if non-null)
// explains why, host-owned, valid only until the next call on the
// calling thread.
```

- **L333:**

```
// Adds a new component of `type` as the last child of the node
// whose id is `parent_id` (pass "" for the document's root).
// `properties_kv` is a flat "key=value;key2=value2" string (no
// escaping of ';'/'=' inside a value -- keep property values that
// need those out of this call, same limitation apply_edit's
// single-string interface already has for its own args). Returns
// true once the proposal is queued.
```

- **L343:**

```
// Edits the existing component whose id is `node_id`: overlays
// `properties_kv` (same flat format as design_add_component)
// onto its current properties, and renames it to `new_id` if
// `new_id` is non-null and non-empty. Returns true once the
// proposal is queued; false (besides the reasons above) if
// `node_id` doesn't match any node in the active document.
```

- **L355:**

```
// Also the docked tab's title, and the id register_panel() uses to
// reject a duplicate. Must outlive the call to register_panel (the
// host copies it internally, so a stack buffer used only for this
// one call is fine).
```

- **L364:**

```
// Fase 6 (see PLAN_settings_panel.md): if true, this panel is not
// docked as its own tab -- the host draws it inline as a section
// inside the built-in "Settings" panel instead, using `name` as the
// section's label. Default false (an ABI <=5 plugin never sets this
// field, so it stays false via zero-initialization -- see the note
// above AVA_STUDIO_PLUGIN_ABI_VERSION).
```

- **L373:**

```
// Handed to the plugin in ava_plugin_init(). Every field is set by the
// host before that call and stays valid until ava_plugin_shutdown()
// returns -- a plugin must not keep using `host` (or anything reached
// through it) after that point.
```

- **L378:**

```
// Set by the host to AVA_STUDIO_PLUGIN_ABI_VERSION at the time
// Ava Studio itself was built. A plugin can check this if it wants
// to feature-detect a newer host at runtime, but the host has
// already validated ava_plugin_abi_version() before calling
// ava_plugin_init() at all -- see plugin_host.cpp.
```

- **L385:**

```
// Reserved for the host's own bookkeeping (Ava Studio's PluginHost
// instance, see plugin_host.cpp). A plugin must never read, write,
// or make any assumption about this field -- it exists purely so
// the host's C function-pointer callbacks (register_panel,
// services.*, which only ever receive `AvaStudioHost*`) can find
// their way back to the C++ object that owns them, without relying
// on `host` being at any particular offset inside that object.
```

- **L397:**

```
// Registers a panel to be docked and drawn every frame from now
// until ava_plugin_shutdown(). Only safe to call from within
// ava_plugin_init() (Fase 0 does not support registering panels
// later, e.g. from inside a draw callback). Returns an opaque
// panel id (>= 0), or -1 on failure -- e.g. `registration->name`
// is already registered, or `registration` is malformed
// (name/draw null).
```

- **L407:**

```
// Every plugin .dll/.so must export exactly these three C symbols
// (extern "C" if the plugin itself is written in C++, so the linker
// doesn't mangle the names).
```

- **L418:**

```
// --- Plugin metadata (Fase 9) --------------------------------------------
// Purely informational, shown next to a plugin's checkbox in the
// "Plugins" menu (see titlebar_panel.cpp) so a person can tell what
// they're enabling/disabling without opening the file itself --
// display name, version, and who made it. Unlike the three symbols
// above, a plugin does NOT have to export any of these: the host
// resolves each independently and simply leaves that field blank in
// the menu if it's missing, never rejects the plugin over it (same
// "optional, additive" spirit as fields appended to AvaUiApi/
// AvaHostServices -- see the ABI note up top -- except these aren't
// even part of the versioned struct, so they don't need an ABI bump
// either).
//
// The returned pointer must stay valid for as long as the plugin's
// library stays loaded (a string literal, e.g. `return "1.2.0";`,
// trivially satisfies this -- don't return a pointer into a buffer
// that could be freed or overwritten).
```

- **L437:**

```
// e.g. "AI Agent (OpenRouter)"
// e.g. "1.3.0" -- any format, shown as-is
// e.g. "Jane Doe"
```


---

## `plugins/plugin_host.h`

- **L3:**

```
// Fase 0 (see PLAN_agente_ia_openrouter.md): Ava Studio's plugin
// system. Scans a `plugins/` folder for .dll/.so files built against
// plugin_api.h, loads them, and keeps the panels they register until
// UnloadAll(). main.cpp owns one PluginHost for the whole session --
// see its construction/LoadAll/UnloadAll calls there.
//
// Deliberately self-contained: unlike AvaHost's plugin_loader.cpp (see
// runtime/avahost/src/plugin/plugin_loader.cpp), this does not reach
// into runtime/avalang/platform's PAL -- that layer's Linux/macOS
// backends are still stubs (see LinLibrary.cpp), and Ava Studio's
// plugin loading has nothing to do with the VM/FFI's own library
// loading, so there is no real benefit to sharing the dependency.
// Windows uses LoadLibrary/GetProcAddress directly, Linux/macOS use
// dlopen/dlsym directly -- see plugin_host.cpp.
```

- **L30:**

```
// One apply_edit() proposal waiting on the person's Aplicar/Rechazar
// decision -- see AvaHostServices::apply_edit in plugin_api.h. Never
// applied automatically; PendingEditsPanel (panels/pending_edits_panel.h)
// is what actually shows these and calls PluginHost::ApproveEdit/
// RejectEdit.
```

- **L36:**

```
// unique for this session, see PluginHost::next_edit_id_
// display name, for the panel's header
// as given by the plugin, relative to the project root
// plugin-provided summary, "" if none
// the file's content on disk at proposal time
// what the plugin wants to replace it with
```

- **L44:**

```
// Result of AvaHostServices::run_project, handed back across
// PluginHost's main-thread mailbox (see RunMailbox below). Mirrors the
// bool-return/out-param shape of the C trampoline one level up in C++.
```

- **L54:**

```
// One panel a loaded plugin registered. main.cpp iterates
// PluginHost::Panels() every frame and docks+draws each one, the same
// way it already calls DrawExplorerPanel/DrawTerminalPanel/etc for the
// built-in panels.
```

- **L67:**

```
// Read-only host state PluginHost forwards into AvaHostServices, wired
// by main.cpp to whatever live objects it already owns (ExplorerState,
// EditorState, TerminalState/EngineBridge) -- PluginHost itself stays
// unaware of those panel-specific types, so it isn't coupled to their
// shape and doesn't need touching if they're refactored later.
```

- **L76:**

```
// Fills `path`/`content` from the active editor tab and returns
// true, or returns false (leaving both untouched) if no real tab
// is active (no tabs open, or the Welcome tab).
```

- **L81:**

```
// Fills `text`/`had_error` from the last Run/compile result and
// returns true, or returns false if nothing has run yet this
// session.
```

- **L86:**

```
// Appends one line to the Output panel's console. PluginHost
// already prefixes `line` with the calling plugin's name before
// invoking this -- see LogTrampoline in the .cpp.
```

- **L91:**

```
// --- Write services (Fase 5) ----------------------------------------
// Writes `edit.new_content` to disk at `project_root/edit.path`
// (creating parent directories for a brand-new file) and, if that
// path is open in a tab, refreshes that tab's buffer too so it
// doesn't show stale text. Called only from the main thread, only
// by PluginHost::ApproveEdit, right after the person clicks
// "Aplicar" in PendingEditsPanel -- never called for a rejected
// edit.
```

- **L101:**

```
// Does the actual work behind AvaHostServices::run_project: runs
// the active tab through the same pipeline the F5 hotkey uses.
// Called only from the main thread, from PluginHost::
// PumpMainThreadWork() -- see RunProjectTrampoline's comment for
// why a plugin's own thread can't just call this directly.
```

- **L108:**

```
// --- Design services (Fase 6) ----------------------------------------
// Fills `path`/`avaui_source` from the active editor tab's CURRENT
// .avaui source and returns true, or returns false (leaving both
// untouched) if the active tab isn't a .avaui document (no tab
// open, the Welcome tab, or a plain .ava/.txt/etc tab). "Current"
// matters here: a .avaui tab's Design view edits `EditorTab::design`
// directly and never touches the TextEditor buffer get_active_file
// reads from, so this always resolves through the same
// WriteAvauiText() conversion ToggleTabViewMode uses when leaving
// Design view -- an edit made through the canvas moments ago is
// never missed just because the person hasn't pressed F7. Reads
// EditorState, which is not thread-safe (same note as
// run_project_on_main_thread above), so this is only ever invoked
// from PluginHost::PumpMainThreadWork() on the main thread, even
// though the design_add_component/design_edit_component
// trampolines that need it run on a plugin's own worker thread --
// see DesignDocMailbox in the .cpp for how that hop happens.
```

- **L128:**

```
// One entry in the "Plugins" menu (see titlebar_panel.h) -- every
// .dll/.so PluginHost can see in plugins_dir, whether or not it's
// currently loaded. Deliberately doesn't try to tell an ABI-mismatched
// or otherwise-rejected file apart from a normal one here -- that
// detail only comes out of an actual LoadAll() attempt (and is already
// logged to the Output panel when it happens); ScanAvailable() is just
// "what files are there and is the user's checkbox for it on".
```

- **L136:**

```
// e.g. "ai_agent.dll" -- same string used in
// StudioSettings::disabled_plugins and
// LoadedPlugin::display_name.
// false if `file_name` is in the disabled
// list passed to ScanAvailable().
// true if this file is currently loaded
// (i.e. present in loaded_ right now). Can
// be false even for an enabled plugin if
// the last LoadAll() rejected it (bad ABI,
// init failed, etc.) -- see the Output
// panel for why in that case.
```

- **L148:**

```
// Fase 9: optional metadata a plugin can export (see plugin_api.h's
// AVA_PLUGIN_DISPLAY_NAME_SYMBOL/VERSION_SYMBOL/AUTHOR_SYMBOL) --
// any of these is "" if the plugin doesn't export that symbol.
// Populated for both loaded and not-currently-loaded plugins alike
// (see ScanAvailable's comment on how it gets this without paying
// for a dlopen every frame), so the "Plugins" menu can show them
// even for a disabled plugin.
// from ava_plugin_display_name(), NOT file_name
// from ava_plugin_version()
// from ava_plugin_author()
```

- **L168:**

```
// Loads every plugin library in `plugins_dir` (non-recursive) that
// exports the three required symbols (see plugin_api.h), reports
// an ABI version this host build understands, AND whose file name
// is not listed in `disabled_plugins` (see StudioSettings::
// disabled_plugins -- a disabled file is skipped with a log line,
// same as any other rejection reason). No-ops (logs nothing,
// doesn't throw) if the directory doesn't exist -- a fresh install
// with no plugins yet is the common case, not an error. One log
// line per plugin: loaded, or the specific reason it was rejected
// (missing symbol / ABI mismatch / init returned false / threw /
// disabled by the user).
```

- **L181:**

```
// Calls ava_plugin_shutdown() on every loaded plugin (reverse load
// order) and unloads each library. main.cpp calls this once,
// before ImGui_ImplOpenGL3_Shutdown() -- a plugin panel must not
// be drawn (or exist in Panels()) after this returns.
```

- **L187:**

```
// UnloadAll() followed by LoadAll(plugins_dir, disabled_plugins) --
// what the "Plugins" menu's checkboxes actually trigger, so a
// toggle takes effect immediately, no restart needed. Same
// main-thread-only rule as UnloadAll()/LoadAll(): must not be
// called while anything is iterating Panels() this frame (main.cpp
// calls it right after reading the titlebar's toggle request, before
// the frame's panel-drawing loop runs -- see main.cpp).
```

- **L196:**

```
// Lists every plugin file PluginHost can see in `plugins_dir` right
// now (non-recursive, same extension filter as LoadAll), without
// loading or unloading anything -- purely for the "Plugins" menu to
// draw its checkboxes from. `disabled_plugins` is the same list
// LoadAll()/Reload() would be given; a name in it comes back with
// PluginInfo::enabled = false. Safe to call every frame (it's just
// a directory listing + a lookup against `loaded_`).
```

- **L206:**

```
// Panels a plugin registered as normal dockable tabs (is_settings
// == false) -- what main.cpp's tab-drawing loop and the View menu
// iterate. Filtered from panels_ on every call (cheap: a handful
// of panels, called once per frame).
```

- **L218:**

```
// Panels a plugin registered as a Settings section (is_settings ==
// true) -- drawn inline inside the built-in Settings panel instead
// of as their own tab. See PLAN_settings_panel.md Fase 1.
```

- **L231:**

```
// Snapshot of every apply_edit() proposal still awaiting a decision,
// oldest first. Safe to call every frame from the main thread --
// copies out under the lock rather than handing back a reference,
// since a plugin thread can push a new one concurrently with
// PendingEditsPanel drawing this list.
```

- **L238:**

```
// Writes the edit to disk (via PluginHostCallbacks::write_approved_edit)
// and removes it from the pending list. No-op if `id` isn't pending
// anymore (e.g. double-click). Main-thread only.
```

- **L246:**

```
// Services one outstanding run_project() request AND one outstanding
// design_add_component/design_edit_component active-document
// fetch, if any -- main.cpp calls this once per frame (see the loop
// in main.cpp). Near-instant no-op on every frame nothing is
// currently blocked inside either of those.
```

- **L259:**

```
// Fase 9: read once at LoadAll() time (the library's already
// open right there, no reason to defer it) so ScanAvailable()
// can hand these back for a loaded plugin with zero extra work.
```

- **L267:**

```
// C function pointers can't capture `this`, so every AvaStudioHost
// callback recovers the owning PluginHost via `host` -- the
// pointer the host hands the plugin is always `&host_` below, a
// member of some live PluginHost instance, so
// reinterpret_cast<PluginHost*>(host) is safe as long as `host`
// really came from us (which it always does -- plugins never
// construct their own AvaStudioHost).
```

- **L282:**

```
// May be called from any thread (see plugin_api.h's comment on
// AvaHostServices::run_project) -- blocks the calling thread on
// run_mailbox_ until PumpMainThreadWork() (main thread only)
// services the request. This is deliberately the *only* place a
// plugin thread waits on the main thread -- apply_edit above never
// blocks, it just queues.
```

- **L291:**

```
// Fase 6 -- both design services end up here: mutate an IComponent
// tree, serialize it, and hand the result to the exact same
// queuing logic ApplyEditTrampoline uses (this is what makes
// design_add_component/design_edit_component share apply_edit's
// approval gate instead of needing a second one -- see the note on
// AvaHostServices::design_add_component in plugin_api.h). Not a
// *Trampoline itself (no direct AvaStudioHost entry point) --
// called from the two below after they've computed `new_content`.
```

- **L302:**

```
// Blocks the calling (plugin) thread until PumpMainThreadWork()
// (main thread only) has run callbacks_.get_active_avaui_document
// -- same hand-off shape as RunProjectTrampoline/run_mailbox_ above,
// via design_doc_mailbox_ instead, since this also has to touch
// EditorState. Returns false if no .avaui tab is active.
```

- **L319:**

```
// Whichever plugin's ava_plugin_init() is currently on the stack --
// RegisterPanelTrampoline stamps this onto RegisteredPanel::
// owner_plugin. Only meaningful during LoadAll()'s call into a
// given plugin; register_panel() is documented (plugin_api.h) as
// only safe to call from within ava_plugin_init(), so this never
// needs to be anything fancier than one field reused per plugin.
```

- **L327:**

```
// Scratch storage the *Trampoline getters return const char*
// into. "Host-owned, valid only for this frame" (see
// plugin_api.h) is honored by simply overwriting these right
// before each call -- a plugin is expected to copy what it needs
// before making another host call, exactly like avahost's C API
// already documents for its own string returns.
```

- **L342:**

```
// Fase 6: single-slot mailbox bridging design_add_component/
// design_edit_component (called from a plugin's own thread) to
// callbacks_.get_active_avaui_document (main thread only, same
// reasoning as RunMailbox above). Only carries the READ side --
// once the plugin thread has `path`/`avaui_source`, the actual
// IComponent mutation + WriteAvauiText() serialization happens
// entirely on that thread (fully local values, no shared state),
// and the result is queued via QueueEdit (pending_edits_mutex_),
// not through this mailbox again.
```

- **L368:**

```
// Single-slot mailbox bridging AvaHostServices::run_project (called
// from a plugin's own thread -- ai_agent's tool-use loop runs on a
// std::thread, see ai_agent_plugin.cpp's SendMessage) to the main
// thread, since actually running a script touches EditorState/
// EngineBridge exactly like the F5 hotkey does, and neither is
// safe to touch from a second thread while ImGui might be reading
// them the same frame. Only one request is served at a time --
// RunProjectTrampoline waits its turn if another is already in
// flight before posting its own, rather than clobbering it.
```

- **L386:**

```
// --- Fase 9: plugin metadata cache -----------------------------------
// ScanAvailable() runs once per frame (see main.cpp) and has to
// report plugin_name/version/author even for plugins that are
// disabled -- i.e. never touched by LoadAll(), so there's no
// LoadedPlugin to read them from. Reading them requires briefly
// dlopen-ing the file, which is too expensive to redo every single
// frame for every disabled plugin. This caches that read per file
// name, keyed additionally by the file's last-write-time so
// dropping in an updated build of a disabled plugin is picked up
// without needing to enable it first.
```


---

## `plugins/plugin_ui_bridge.cpp`

- **L13:**

```
// Full definition lives only here -- plugin_api.h only forward-declares
// `AvaPanelContext` as an opaque type, so a plugin (which never
// includes this file) has no way to look inside it even though it's
// handed the pointer on every UI call.
```

- **L18:**

```
// SameLine() (below) doesn't call ImGui::SameLine() immediately -- it
// just remembers that the *next* widget asked to share the previous
// line, so that widget's own call can decide whether there's actually
// room for it. A narrow docked panel (see the "Plugins" panels docked
// at AVA_DOCK_BOTTOM, which end up sharing width with Explorer/Editor/
// Output) is exactly the case where an input_text()+same_line()+
// button() row -- a completely reasonable thing for a plugin to write
// -- doesn't fit, and ImGui's own SameLine() has no built-in concept
// of "wrap to a new line instead"; it just lets the next item's box
// extend past the window's right edge, clipped by the parent
// scroll region with no way back to it short of a horizontal
// scrollbar (bad UX for reading text/using a form either way).
```

- **L32:**

```
// Called from every widget function that could legitimately follow
// same_line() (button/input_text/input_text_multiline/combo).
// `needed_width`: how much horizontal room this widget wants on the
// current line to render fully -- pass 0 to always honor the same-line
// placement without a wrap check (used for widgets where "just shrink
// a bit" reads fine, unlike a button/label whose text would be cut).
```

- **L43:**

```
// Not enough room left on this line -- drop to a new one
// instead of drawing (and clipping) past the panel's edge.
```

- **L49:**

```
// Minimum width a text field/combo needs to still be usable once
// shrunk -- below this it's easier to read on its own line than
// squeezed next to whatever came before it. Buttons/labels use their
// exact rendered width instead (see ConsumePendingSameLine's callers
// below), since those can't shrink at all.
```

- **L56:**

```
// Room reserved on the input's own line for the send/stop button that
// always follows input_text_multiline_submit() (see its header comment
// -- it exists specifically for that "type + Enviar" chat row). Sized
// to "Detener" (the longer of the two labels actually used) plus frame
// padding and the SameLine gap -- any more than that just leaves a gap
// between the input and the button instead of the button sitting right
// after it.
```

- **L70:**

```
// AvaPanelContext's definition must be visible wherever plugin_api.h's
// forward declaration is used with a real member access -- since that
// forward declaration lives in the (extern "C") global namespace, the
// definition does too.
```

- **L95:**

```
// A button can't shrink -- it needs its full rendered width (label
// text + the frame padding ImGui adds on both sides) or it wraps to
// its own line instead of getting clipped mid-label (see the
// "Saludar" -> "Saluc" clipping this fixes).
```

- **L118:**

```
// Dear ImGui has no built-in "Enter submits, Shift+Enter inserts a
// newline" mode for multi-line inputs -- only a Ctrl+Enter variant
// (ImGuiInputTextFlags_CtrlEnterForNewLine), which isn't the chat
// convention every user already knows from Slack/Discord/etc. This is
// the standard ImGui workaround: a CallbackCharFilter runs *before* a
// keystroke is applied to the buffer, so it can inspect the '\n' that
// Enter produces and either swallow it (plain Enter -- flag it as a
// submit instead of a newline) or let it through (Shift+Enter, held at
// the moment the callback fires -- same frame as the keypress).
```

- **L145:**

```
// -1.0f (full available width) would leave nothing for the send/stop
// button that's always drawn right after this via same_line() --
// that button would then wrap to its own line below instead of
// sitting beside the input like every other chat app. Reserve room
// for it here instead, and only fall back to full width if the
// panel is too narrow for that to make sense anyway.
```

- **L160:**

```
// Dear ImGui's hint overlay (InputTextWithHint) only exists for the
// single-line widget -- there's no multiline equivalent to call, so
// draw the greyed placeholder by hand on top of the box, in the
// same spot InputTextWithHint puts it, only while empty and not
// focused (so it never fights with the real caret/text).
```

- **L172:**

```
// The caller (SendMessage) clears `buffer` right after this
// returns, but this widget is still the active one -- ImGui
// won't pick up that external change until it reactivates.
// SetKeyboardFocusHere(-1) re-targets the item just drawn
// (this input) for activation next frame, which is what makes
// it redraw from the now-empty buffer instead of showing its
// own stale internal copy. Same pattern as terminal_panel.cpp's
// console input.
```

- **L210:**

```
// Manually wraps `text` to `wrap_width`, inserting '\n' at the same break
// points ImGui::TextWrapped() would pick (ImFont::CalcWordWrapPositionA is
// the same helper TextWrapped uses internally) -- unlike TextWrapped,
// InputTextMultiline has no wrap mode of its own: every line already in
// the buffer is treated as one visual line verbatim. Without this, a
// message with no embedded '\n' rendered as one single line that either
// scrolled sideways or, with NoHorizontalScroll set (see
// selectable_message below), just got clipped at the edge -- which is
// exactly the "no salto de línea, se sale del chat" bug this fixes.
```

- **L237:**

```
// Skip the single space that caused the break, same as
// TextWrapped does -- otherwise every wrapped line but the
// first would start with a stray leading space.
```

- **L249:**

```
// Fase 10: see plugin_api.h's comment on selectable_message for the
// "why" -- this is what actually renders it. A read-only
// InputTextMultiline instead of TextWrapped is the standard Dear ImGui
// way to get real selection/copy out of a block of text; TextWrapped
// has no selection model at all.
```

- **L266:**

```
// Wrapped once here (not left to the widget, which can't do it --
// see WordWrapText's comment) and reused for both what's actually
// drawn and the height measurement below, so the box is always sized
// to exactly the lines it displays, hard-wrapped included.
```

- **L282:**

```
// ReadOnly: this is a copyable label, not an editable field -- the
// goal is letting the user drag-select/Ctrl+C their own chat
// history, not rewrite it. const_cast is safe here: ImGui never
// writes through a ReadOnly buffer, it only needs a non-const
// pointer to match the same InputText signature used by editable
// callers.
```

- **L308:**

```
// Deferred -- see g_pending_same_line's comment above. The actual
// ImGui::SameLine() call happens inside whichever widget function
// is called next, once that widget knows whether it actually has
// room.
```

- **L330:**

```
// Only meaningful while a scroll region is open (between
// BeginChild/EndChild above) -- same requirement ImGui itself has
// for GetScrollMaxY()/SetScrollHereY().
```

- **L362:**

```
// Reused across calls rather than heap-allocated per panel: panels
// are drawn synchronously, one at a time, on the main thread (see
// main.cpp's loop over PluginHost::Panels()) -- each Begin/End
// pair fully completes before the next panel's starts, so a single
// reused instance is safe and avoids an allocation every frame per
// panel. ImGui::PushID scopes every widget id inside this panel
// (its "##input"/"##combo"/etc defaults above) so two different
// plugins' panels never collide.
```

- **L372:**

```
// Defensive reset: if a misbehaving plugin's draw() calls
// same_line() as its very last UI call (nothing to actually place
// next to), g_pending_same_line would otherwise leak into the next
// plugin panel drawn this frame and wrongly glue its first widget
// onto whatever line this panel last drew.
```


---

## `plugins/plugin_ui_bridge.h`

- **L3:**

```
// Host-side implementation of AvaUiApi (see plugin_api.h) -- the only
// file in Ava Studio that both includes imgui.h AND plugin_api.h.
// PluginHost wires an AvaStudioHost's `ui` field to these functions'
// addresses (see plugin_host.cpp); main.cpp calls FillUiApi once at
// startup and BeginPanelContext/EndPanelContext around each registered
// panel's draw() call every frame.
//
// AvaPanelContext itself carries nothing plugins can act on beyond
// passing the pointer back into these calls -- host-side it's just the
// currently-drawing panel's name, used to PushID/PopID so two
// different plugins' panels can each use e.g. "##input" as a widget id
// without colliding.
```

- **L25:**

```
// main.cpp brackets a registered panel's draw() call with these two --
// see plugin_host.h's RegisteredPanel and the main.cpp loop that walks
// PluginHost::Panels(). Must be called between ImGui::Begin(name) and
// ImGui::End() for that same panel.
```


---

## `syntax/syntax_avalang.h`

- **L59:**

```
// --- Comments (DESPUÉS de strings para evitar que # en strings se marque como comment) ---
// El regex #[^\n]* coincide con # hasta fin de línea
// Al procesarse después de los strings regex, los # dentro de strings no se marcan
```


---

## `theme.cpp`

- **L13:**

```
// "Ava amber" -- orange-dominant theme. Chrome (backgrounds, borders,
// panels) uses a neutral, almost-pure-black palette in the style of
// terminal-style IDEs like OpenCode -- cool near-black instead of the
// previous warm brown -- so the orange accent (buttons, active tab,
// syntax highlighting) is the only thing carrying color and really
// pops against it.
// #0b0b0d
// #101013
// #0e0e10
// #1c1c20
// #29292e
// #e87a34
// #ff984a
// #c26021
// #0b0b0d (same as editor -- active tab blends into content)
// #101013
// #e6e6e8
// #6b6b70
```


---

## `theme.h`

- **L3:**

```
// Applies a dark theme modeled on VSCode's "Dark+" palette (colors,
// rounding, spacing) so Ava Studio feels immediately familiar to
// anyone coming from VSCode. Call once after ImGui::CreateContext().
```


---

## `util/ava_cli_locator.cpp`

- **L36:**

```
// ava_studio.exe and ava_cli.exe land under the same build_cli/runtime/
// tree but in different target subfolders (runtime/avastudio/<Config>/
// vs. runtime/avalang/<Config>/, the latter forced by
// runtime/avacli/CMakeLists.txt's RUNTIME_OUTPUT_DIRECTORY override --
// see the comment there). Try every layout that produces: multi-config
// generators (VS, with a Release/Debug subfolder) AND single-config ones
// (Ninja/Make, no subfolder), covering both a same-name-as-self-dir
// config folder and a config-less one.
```


---

## `util/ava_cli_locator.h`

- **L7:**

```
// Directory containing the currently running executable (ava_studio.exe
// itself), not the cwd. Used by DetectAvaCliPath() below. Exposed in
// case a future caller needs "next to me" resolution for something else
// that isn't ava_cli.
```

- **L13:**

```
// Best-effort guess at where ava_cli(.exe) lives, assuming the common
// build layouts (see the .cpp for the exact candidates tried): same
// folder as ava_studio.exe, or a sibling `avalang/` output folder from
// the same CMake build tree. Returns an empty path if nothing was
// found -- callers should fall back to an explicit user-configured path
// (see StudioSettings::build_ava_cli_path) in that case, same convention
// used for the Build panel's own "ava_cli path" setting.
```


---

## `util/csv.h`

- **L8:**

```
// Parser CSV mínimo pero correcto (RFC4180): soporta campos entre comillas
// dobles, comas y saltos de línea reales dentro de un campo citado, y ""
// como forma de escapar una comilla literal dentro de un campo citado.
// Usado por keyword_docs.cpp/builtin_signatures.cpp para leer
// data/docs/keyword_docs.csv y data/docs/builtin_signatures.csv sin depender de una
// librería externa.
//
// No hace ninguna interpretación semántica de las celdas -- en particular,
// NO desescapa "\n" literal (barra + n) a salto de línea real; eso es
// responsabilidad del caller (ver UnescapeCell más abajo), porque solo el
// caller sabe qué columnas de qué archivo usan esa convención.
//
// Devuelve una fila vacía por cada línea en blanco del archivo (útil para
// separar visualmente secciones al editar a mano); el caller debería
// saltarlas si corresponde.
```

- **L25:**

```
// Arma una línea CSV bien formada a partir de campos crudos: envuelve en
// comillas cualquier campo que contenga coma, comilla o salto de línea, y
// duplica las comillas internas. Usado por tools/dump_docs.cpp para
// generar los CSV iniciales a partir de las tablas hardcodeadas de hoy.
```

- **L31:**

```
// Convención compartida por ambos CSV: dentro de una celda, "\n" (dos
// caracteres, barra invertida + n) representa un salto de línea real en
// el texto mostrado en el tooltip. Se usa una barra literal en vez de un
// salto de línea real embebido para que una fila = una línea física del
// archivo (más fácil de leer/diffear/editar a mano que una celda citada
// multi-línea).
```

- **L40:**

```
// Segunda convención, solo para columnas que representan una lista de
// variantes/valores dentro de UNA celda ya citada por ParseCsv (ej. las
// dos formas de sintaxis de `while`, o la lista de parámetros de un
// builtin): separador "|||" para variantes de sintaxis completas, "|"
// simple para listas cortas de tokens (parámetros). Ninguno de los dos
// aparece nunca en el contenido real de AvaLang, así que no hace falta
// escapar nada adicional.
```


---

## `util/data_dir.h`

- **L7:**

```
// Resuelve la carpeta "data" junto al ejecutable (no el cwd del proceso --
// mismo razonamiento que ResolveWorkspaceDir en main.cpp para "scripts/":
// dónde vive el .exe es estable sin importar cómo se lo haya lanzado
// -doble click, acceso directo, debugger-, el cwd no). Ahí es donde viven
// keyword_docs.csv y builtin_signatures.csv (en data/docs/) -- el usuario los puede editar
// con cualquier editor de texto sin recompilar Ava Studio.
//
// Devuelve la ruta con "/" al final. No crea el directorio -- a diferencia
// del workspace de scripts, si "data" no existe o los CSV no están, el
// llamador debe caer a la tabla embebida (ver DefaultKeywordDocs() /
// DefaultBuiltinSignatures()) en vez de fallar.
```

- **L20:**

```
// Default "base modules" folder, next to the executable (same reasoning
// as ResolveDataDir): "<exe_dir>/modules". This is what the Properties
// dialog's blank/unset path resolves to at runtime -- the field itself
// stays empty so settings.ini never bakes in a machine-specific absolute
// path (see util/settings.h); the user can still point it anywhere via
// Properties, and that explicit choice is what actually gets persisted.
// No trailing separator.
```

- **L29:**

```
// Lee un archivo completo como texto. Devuelve false (sin tocar `out`) si
// no se pudo abrir -- "no existe" no es un error real acá, es la señal de
// "usá el fallback embebido".
```


---

## `util/i18n.cpp`

- **L24:**

```
// data/langs/en.csv, data/langs/es.csv columns: key,value -- same
// two-column shape data/docs/keyword_docs.csv uses for its own name
// column, parsed with the same
// util::ParseCsv so a translator can edit these in a spreadsheet without
// touching code.
```

- **L67:**

```
// Reloads `table` from disk on the first call for this locale, or when
// the CSV's mtime moved forward since the last successful load -- same
// hot-edit idiom KeywordDocs()/BuiltinSignatures() already use. A
// missing/empty CSV is not an error: langs/en.csv/langs/es.csv start
// out with only a couple of test keys (see PLAN_AVASTUDIO_IDE.md,
// phase i18n-0), so most keys resolve through Tr()'s "[key]" fallback
// until each panel migrates to Tr() in its own later phase.
```

- **L90:**

```
// No CSV yet and nothing previously loaded -- leave entries
// empty rather than inventing a hardcoded fallback table: unlike
// KeywordDocs(), there's no pre-i18n content to fall back to
// here, every key is new.
```

- **L132:**

```
// Missing everywhere -- return "[key]" so it's visibly wrong in the
// UI instead of silently blank or a crash. One cached entry per
// missing key (not a single shared buffer) so a caller holding this
// reference across a frame doesn't see it mutate out from under them
// if a *different* missing key gets looked up right after.
```


---

## `util/i18n.h`

- **L7:**

```
// The two locales AvaStudio ships with today -- see PLAN_AVASTUDIO_IDE.md
// section 6.4 for why English and Spanish specifically (the two that
// already coexist, inconsistently, in the current hardcoded strings).
// English is the canonical key language: a key missing from the active
// non-English locale falls back to it before falling back to the raw key.
```

- **L17:**

```
// Parses a persisted StudioSettings::language value ("en"/"es") into a
// Locale. Anything else (including an empty string, e.g. on first run)
// falls back to fallback_locale rather than treating it as an error --
// mirrors how a missing settings.ini key already falls back to a
// StudioSettings default elsewhere.
```

- **L27:**

```
// Sets the locale every subsequent Tr() call translates into. Safe to
// call every frame (e.g. right after a Settings combo change) -- it's a
// cheap enum compare, not a re-parse; the underlying CSV load for a
// locale only happens the first time that locale is actually needed.
```

- **L34:**

```
// Translates `key` using data/langs/en.csv / data/langs/es.csv for
// the active locale (see i18n.cpp for the load/reload mechanics, same
// mtime-check idiom as KeywordDocs()). Falls back to English if `key`
// is missing from a non-English active locale, and to "[key]" -- shown
// on purpose -- if it's missing from English too, so an untranslated
// string is visible in the UI during development instead of failing
// silently or crashing.
```


---

## `util/log_bridge.h`

- **L8:**

```
// One line in the Output panel's general log (panels/logs_panel.h) --
// separate from EngineBridge::ConsoleLine, which is only ever a running
// script's own stdout/errors. See LogBridge below for why these two
// streams are split.
```

- **L16:**

```
// Host-side log sink for everything that *isn't* a script's own
// print()/compile output: plugin lifecycle messages (loaded/initialized),
// PluginHost applying/rejecting an edit, future engine/IO diagnostics,
// etc.
//
// This used to just be EngineBridge::LogExternal(), writing straight
// into the same scrollback as RunScript()'s stdout -- that made the
// Output panel a mix of "what my script printed" and "what the studio's
// plugins are doing in the background", with no way to tell which was
// which at a glance (see the Output/Terminal split this type exists
// for). LogBridge is intentionally its own tiny type instead of a new
// EngineBridge method: this stream has nothing to do with the AvaVM, so
// it shouldn't live on the same object that owns one.
//
// main.cpp owns one LogBridge for the whole session, same lifetime as
// EngineBridge, and hands it to PluginHost's callbacks and to
// DrawLogsPanel() (panels/logs_panel.h).
```


---

## `util/process_log.h`

- **L9:**

```
// Forwards whatever's new in `log` (since `forwarded_upto`, a byte
// offset into it) to the Output panel, one LogBridge line at a time --
// meant to be called every frame while a background process is running
// (as well as once more right after it finishes), so output shows up in
// Output in near real time instead of as a single dump at the end. Only
// forwards *complete* lines (up to the last '\n') unless
// `flush_partial_tail` is set, in which case a trailing line with no
// newline yet (the very last bit of output once the process has
// actually exited) is forwarded too.
```


---

## `util/settings.cpp`

- **L15:**

```
// Mirrors the config_dir resolution in main.cpp's ini_path setup (kept
// separate/duplicated on purpose -- this is a small, self-contained file
// and main.cpp's lambda is tied to ImGui's io.IniFilename lifetime, not
// worth threading a shared helper through for ~10 lines).
```

- **L41:**

```
// Trim helper: '\r' shows up if the file was ever edited/saved on
// Windows with CRLF, and stray whitespace around '=' is harmless to
// tolerate for a hand-editable file.
```

- **L54:**

```
// Blank on a first run (no settings file yet) -- see the comment on
// StudioSettings::modules_path for what blank means at the point of
// use. Deliberately NOT util::ResolveDefaultModulesDir() here: that
// would bake today's exe location into settings.ini as soon as it's
// saved, defeating the portability blank is meant to give.
```

- **L75:**

```
// One "disabled_plugin=<file_name>" line per disabled
// plugin, rather than a single comma-joined value -- a
// file name can't contain '\n', so this never needs
// escaping, unlike a comma-separated list would if a
// plugin file name ever had a comma in it.
```


---

## `util/settings.h`

- **L8:**

```
// User-configurable, persisted-across-sessions settings for Ava Studio.
// Grow this struct as more Settings-dialog options show up (see
// panels/titlebar_panel.cpp's Properties modal).
```

- **L12:**

```
// UI locale, as util::LocaleToString() renders it ("en"/"es"). Empty
// ("") is the default on a first run -- see util::LocaleFromString(),
// which treats it (and anything else it doesn't recognize) as
// English rather than an error. Applied via util::SetLocale() right
// after LoadSettings() in main.cpp, and every time the General
// section's language combo changes (see settings_panel.cpp).
```

- **L20:**

```
// Base modules folder passed to ava_vm_set_stdlib_path -- see the
// module-resolution order comment in public/include/avalang.h.
// Empty ("") is the default, both on a first run and whenever the
// user clears the Properties field -- it means "use the modules/
// folder next to the executable" (see
// util::ResolveDefaultModulesDir()), resolved at the point of use
// rather than baked into settings.ini, so the settings file stays
// portable across machines/install locations. A non-empty value is
// an explicit user override and is used as-is.
```

- **L31:**

```
// File names (e.g. "ai_agent.dll", "hello_world.so" -- same string
// as PluginHost::PluginInfo::file_name) of plugins the user turned
// off from the "Plugins" menu (see titlebar_panel.h). A file NOT in
// this list is enabled -- that way a plugin dropped into plugins/
// for the first time defaults to on, instead of every existing
// settings.ini needing to know about it in advance. main.cpp passes
// this straight into PluginHost::LoadAll/Reload.
```

- **L40:**

```
// Names of panels the user closed -- either a plugin panel
// (RegisteredPanel::name, e.g. "AI Agent") via its own tab X, the
// "Plugins" modal's panel list, or the "View" menu, OR a built-in
// panel (see panels/builtin_panels.h, e.g. "Explorer") via its tab X
// or the "View" menu. A name NOT in this list is open -- same
// "absence means default" convention as disabled_plugins, so a
// panel a plugin registers for the first time defaults to visible.
// Distinct from disabled_plugins: closing a panel just hides its
// tab, it doesn't unload the plugin (AvaHostServices calls the
// plugin makes in the background, e.g. apply_edit proposals, keep
// working). main.cpp is what actually reads/writes this map at
// runtime (see `panel_open` there); this is only the persisted form.
```

- **L54:**

```
// --- "Build" panel (see panels/build_panel.h) --------------------
// Packages the current project into a distributable .exe by
// shelling out to `ava_cli build` (runtime/avacli), which drives
// runtime/avapack. Persisted so re-opening Ava Studio doesn't need
// every field re-entered -- most only need to be set once per
// machine/checkout. Empty string means "not set yet / auto-detect",
// documented per-field in build_panel.cpp.
```

- **L62:**

```
// --project. "" = use the Explorer panel's open folder.
// --entry, relative to build_project_dir. "" = auto-detect main.ava.
// --out (directory mode). "" = <project>/dist.
// --repo-root, the AvaLang repo checkout. "" = auto-detect.
// Path to ava_cli(.exe). "" = auto-detect next to ava_studio.exe.
// --key-file (optional, 32 raw AES-256 bytes). "" = random key per build.
// VCPKG_ROOT env var for ava_cli's cmake configure step (see
// install.bat). "" = use the VCPKG_ROOT already in the
// environment, else auto-detect <repo_root>/vcpkg.
```

- **L72:**

```
// --target passed to ava_cli build -- "desktop" (default) or
// "barekernel", see build_command.cpp's --help and §3.4.2 of
// PLAN_AVASTUDIO_IDE.md. Deliberately separate from the *platform*
// the resulting binary runs on (Windows/Linux/macOS, see §3.4.1):
// avapack doesn't cross-compile, so the platform is always just
// whatever OS ava_studio.exe itself is running on -- not a setting,
// just a read-only, computed-at-draw-time label in build_panel.cpp.
// Empty ("") on a first run is treated as "desktop", same
// "absence means default" convention as the other build_* fields.
```

- **L83:**

```
// --toolchain-dir, only meaningful (and only shown/required in the
// UI) when build_target == "barekernel" -- folder containing the
// i686-elf cross-compiler (gcc/g++/ld/objcopy/nm), see
// build_command.cpp's --help. "" = not set; a barekernel build
// can't proceed without it (no auto-detect -- there's no
// reasonable default location for a cross-toolchain).
```

- **L91:**

```
// --obfuscate
// --obfuscate-strings (requires build_obfuscate)
// --flatten-control-flow (requires build_obfuscate)
// --zero-disk
// --debug (NOT for distribution, see avapack/README.md)
```

- **L98:**

```
// Loads persisted settings from the per-user config folder (same place
// as imgui.ini -- %APPDATA%/AvaStudio on Windows, ~/.config/AvaStudio
// elsewhere). Missing file or missing keys fall back to defaults, so
// this is always safe to call on startup even on a first run.
```

- **L104:**

```
// Persists `settings` to that same per-user config folder. Best-effort:
// silently does nothing if the folder can't be created/written (e.g. a
// locked-down install), same philosophy as imgui.ini's own save.
```

