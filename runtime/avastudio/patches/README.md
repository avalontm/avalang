# patches/

## imguicolortextedit_interpolation.patch

`ImGuiColorTextEdit` (goossens fork, `studio/CMakeLists.txt` -> `FetchContent`)
doesn't support:

1. A dedicated color for the content of `{expr}` inside an f-string
   (`$"..."`) -- the whole string, interpolation included, is painted
   with a single `Color::string`. There's no hook (`customTokenizer`)
   that gets invoked *inside* a string state, only at the plain-text
   level.
2. Along the way, we found that the library's own `inOtherString`
   state (the one used for strings delimited by custom sequences,
   like our `$"..."`) has its own bug: it paints the content with
   `Color::comment` instead of `Color::string`.

This patch:
- Adds `Color::interpolation` to the enum and to the default
  dark/light palettes (Ava Studio overrides them anyway in
  `InitEditorPanel`).
- Makes the `inOtherString` state track `{`/`}` nesting and paint
  that range with `Color::interpolation`; a `"` inside an open
  interpolation no longer closes the string early.
- Along the way, fixes the bug above (`comment` -> `string` color).

## imguicolortextedit_import_path.patch

`ImGuiColorTextEdit` has no `Color::...` value for "a module path in an
import statement" -- upstream (and our other patches) don't need one,
so there's nowhere to point AvaLang's `import x.y as z` path segments
if we want them colored differently from the `import`/`as` keywords
and from plain identifiers.

This patch:
- Adds `Color::importPath` to the enum and to the default dark/light
  palettes (Ava Studio overrides them anyway in `InitEditorPanel`, same
  as `interpolation`).

That's it -- no colorizer changes. Unlike `interpolation` or
`docComment`/`docParamTag`, this one doesn't need a state machine
inside `TextEditor::Colorizer::update()`: AvaLang's own
`customTokenizer` (`avalang_language.cpp`) already runs for every
plain-text token and can return `Color::importPath` directly for the
`NAME ('.' NAME)*` segments after `import`, the same way it already
returns `Color::identifier`/`Color::preprocessor` for interface/class
names in a heritage clause. See `AvaLangTokenizer`'s
`kExpectImportPath`/`kExpectMoreImportPath` states.

## imguicolortextedit_interface_name.patch

Reported bug: text typed in the editor defaulted to green instead of
white, and only turned another color once the tokenizer recognized
it as something specific.

Root cause: `Color::identifier` is upstream's own default color for
*any* plain identifier the colorizer doesn't otherwise recognize --
every variable, parameter, or name you type falls through to this
slot when nothing more specific claims it (see `renderText()`'s
fallback). AvaLang's `customTokenizer`
(`avalang_language.cpp::AvaLangTokenizer`) was *also* returning
`Color::identifier` for a known interface name (`class X : IFoo`,
`interface IFoo`), and `InitEditorPanel()` mapped that single slot to
`kSynInterface` (green). Same slot, two different meanings -- so
everything you typed inherited the green meant only for interfaces.

This patch:
- Adds `Color::interfaceName` to the enum and to the default
  dark/light palettes (Ava Studio overrides them anyway in
  `InitEditorPanel`, same as `importPath`).

No colorizer changes, same shape as `imguicolortextedit_import_path.patch`:
`AvaLangTokenizer` already runs for every plain-text token, so its
`kExpectClassName`/`kExpectBaseName` states now return
`Color::interfaceName` instead of `Color::identifier` for interface
names, leaving `Color::identifier` free to mean what upstream
intended -- the default text color -- and `InitEditorPanel()` now
maps `Color::identifier` to `kSynIdentifier` (plain white,
`palette::kSynVariable`) and `Color::interfaceName` to
`kSynInterface` (green) separately.

## imguicolortextedit_variable_name.patch

Follow-up to `imguicolortextedit_interface_name.patch`: `Color::identifier`
was freed to mean "plain default text" (white), but AvaLang had no slot to
point to for "this identifier is a variable being declared or assigned" --
so a variable and a plain, unanalyzed identifier looked identical.

This patch:
- Adds `Color::variableName` to the enum and to the default dark/light
  palettes, same shape as `interfaceName`/`importPath`.

No colorizer changes -- `AvaLangTokenizer` (`avalang_language.cpp`) now
returns `Color::variableName` instead of leaving the token unhandled
(falling through to `Color::identifier`) when it recognizes one of these
patterns, tracked with a `paren_depth_` counter (incremented/decremented
on `(`/`)`) so a named call argument (`foo(x = 1)`) isn't mistaken for a
declaration:
- `NAME '='` (not `==`), at paren depth 0 -- `assignStatement`.
- `NAME` followed by `+=`, `-=`, `*=`, `/=`, `%=`, or `//=` -- `augAssignStatement`.
- `NAME 'as' NAME` -- `typedAssignStatement`/`typedDeclStatement`, at any
  paren depth (this pattern is unambiguous even inside a parameter list,
  since call-site named args never use `as`).
- `NAME 'in'` -- the loop variable in `for NAME in exprList`.

### Known limitations of this detection
- Only the last name before `=`/`in` is caught in `a, b = 1, 2` or
  `for a, b in items` (multi-target assignment/for) -- the earlier names
  stay the default color.
- A parameter default without a type annotation (`func foo(x = 1)`) is
  indistinguishable from a named call argument with the same syntax
  (`foo(x = 1)`) under a single-token lookahead, so it's left uncolored
  (default) rather than risk miscoloring call sites. Typed parameters
  (`func foo(x as int = 1)`) are still caught via the `as` check.
- `obj.attr = x` colors `attr` as a variable too (the tokenizer doesn't
  track that a `.` preceded it) -- harmless, since it's still an
  assignment target.

## Known limitations (documented, not surprise bugs)
- `{}` counting is per line (it resets on every new line). An
  f-string with an interpolation that spans a line break without
  closing (`{` on one line, `}` on the next) will color the
  following line incorrectly. This shouldn't happen with normal
  FSTRING usage in AvaLang.
- The content *inside* `{...}` is painted entirely with
  `Color::interpolation`, without being re-tokenized as AvaLang code
  (i.e. a nested string inside the interpolation, `{f("x")}`, isn't
  recolored as a string). Doing this properly would require a
  recursive sub-lexer; out of scope for this patch.

## Why a patch and not a fork
`studio/CMakeLists.txt` pins `GIT_TAG master` (see PROGRESS.md/AvaStudio.md
about version pinning). A patch applied via `PATCH_COMMAND` in
`FetchContent_Declare` is easier to maintain/review than forking the
entire repo for this one change. If a specific commit hash ever gets
pinned and that commit changes these lines, the patch may stop
applying cleanly -- in that case, CMake will fail at the configure
step with a `git apply` error, not silently.

## imguicolortextedit_bold_keywords.patch

`ImGuiColorTextEdit` takes a single `ImFont` per call to `Render()`
(`font = ImGui::GetFont()`, see `renderText()`) and uses it for *all*
glyphs regardless of their `Color::...` -- there's no way to ask for
"this in bold, this in regular" without touching the library, neither
via `Language` nor via `customTokenizer` (that hook only returns a
`Color`, not a font weight).

This patch adds:
- `TextEditor::SetBoldFont(ImFont*)` and `SetBoldColors(std::initializer_list<Color>)`
  -- an optional second font (`boldFont`) and a set of which
  `Color::...` values should use it (`boldColors`, a
  `std::array<bool, Color::count>`).
- In `renderText()`, the final `else` that calls
  `font->RenderChar(...)` now picks `boldFont` instead of `font` when
  `boldColors[glyph.color]` is `true`. If `boldFont` is `nullptr`
  (`SetBoldFont` was never called) or the color isn't marked, the
  behavior is identical to upstream.

Ava Studio uses this in `InitEditorPanel()` so that
`keyword`/`declaration` (if/while/func/true/false/nil/...) render
with JetBrains Mono Bold while the rest of the code (strings,
identifiers, comments) stays in the regular font -- see
`src/panels/editor_panel.cpp` and `src/fonts/embedded_font.{h,cpp}`
for where that second font comes from.

### Why not "bold the whole code" instead of this
This was evaluated and dropped: bold across all editor text doesn't
need a patch (a `PushFont`/`PopFont` from ImGui around
`editor.Render()` is enough), but a uniform weight across all code
tires the eye in long sessions and reduces contrast between
similar-looking letters at 16px (`rn` vs `m`, `cl` vs `d`). Selective
bold on keywords gives the emphasis that was wanted without that
cost -- at the cost of needing this patch.

## Fix 2026-07: `Color` moved inside the class (GIT_TAG master)

As warned above, pinning `GIT_TAG master` means an upstream change
that reorders the file can silently break the patch (`git apply`
applies cleanly because the hunk's context didn't change, but the
result doesn't compile). That's what happened here:
`imguicolortextedit_bold_keywords.patch` inserted
`SetBoldFont()`/`SetBoldColors(std::initializer_list<Color>)` right
after `Render()` (around line 120 of the header), assuming `Color`
was already declared at that point in the class. Upstream moved the
`enum class Color` section further down (now after
`GetLightPalette()`, ~line 330+), so by the time the compiler
reached `SetBoldColors(std::initializer_list<Color>)` the `Color`
type didn't exist yet -> `error C2065: 'Color': undeclared
identifier` (MSVC) when compiling `text_editor.vcxproj`.

Fix: the patch now inserts `SetBoldFont()`/`SetBoldColors()`
immediately after `GetLightPalette()` -- i.e. after `enum class
Color` and `class Palette` are already fully declared -- instead of
right after `Render()`. The rest of the patch (the change in
`renderText()` in `TextEditor.cpp` and the `boldFont`/`boldColors`
fields in the `protected` section, which already lived after
`Color`) didn't change.

If this breaks again for the same reason, the symptom is always the
same: `git apply` doesn't fail, but MSVC/GCC throws "undeclared
identifier" on `Color` at the line where `SetBoldColors` was
inserted. Fix: move the inserted block to a point in the class that
comes *after* the `enum class Color` declaration in upstream's
current header (look for `enum class Color : char` in
`TextEditor.h`).

## imguicolortextedit_doc_comment.patch

`ImGuiColorTextEdit` only knows a single flat color,
`Color::comment`, for anything matching
`Language::singleLineComment` -- there's no way for a doc-comment
block (`##` in AvaLang) to be painted differently from a regular
comment (`#`), nor to highlight a specific token (`@param name`)
inside that block, without touching the library.

This patch adds:
- `Color::docComment` and `Color::docParamTag` to the enum and to
  the default dark/light palettes (Ava Studio overrides them anyway
  in `InitEditorPanel`, see `src/panels/editor_panel.cpp`).
- `Language::docCommentPrefix` (AvaLang sets it to `"##"`, see
  `src/languages/avalang_language.cpp`), checked in
  `Colorizer::update` **before** `singleLineComment` -- it has to go
  first because `"##"` also matches the shorter `"#"` prefix, and
  the original check would have matched first.
- Inside that block, a word-by-word scan that looks for `@param` and
  repaints that tag (plus the parameter name that follows it, if
  any) with `Color::docParamTag`, leaving the rest of the block in
  `Color::docComment`.
- `Autocomplete::updateState` now also counts `docComment` and
  `docParamTag` as "I'm inside a comment" (it previously only looked
  at `Color::comment`), so autocomplete isn't offered inside a
  doc-comment block -- same behavior it already had for a regular
  `#`.

### Why a new prefix (`docCommentPrefix`) instead of reusing `singleLineCommentAlt`
The library already has `singleLineCommentAlt`, a second single-line
comment prefix -- but it paints it with the same old
`Color::comment` (see the corresponding branch in
`Colorizer::update`), it doesn't give it its own color. Reusing it
would have meant patching that branch anyway, so a dedicated field
with clearer semantics was added instead
(`docCommentPrefix`, rather than "the alternate prefix, but this
time actually colored differently").

## Fix 2026-07: `GIT_TAG` pinned to `Legacy` instead of `master`

`master` started drifting (upstream is rewriting the editor from
scratch on its `future` branch; its own README says that at some
point `master` will jump to that new architecture, "breaking
backwards compatibility"). Before that, a minor change on `master`
had already shifted the lines of `TextEditor.cpp` around
`Colorizer::update` (~line 3647) enough that `git apply` failed with
"patch does not apply" when applying
`imguicolortextedit_interpolation.patch` (first hunk, `@@
-3647,...`).

`studio/CMakeLists.txt` now pins `GIT_TAG Legacy` -- the release tag
that the author (goossens) himself published as a frozen snapshot of
the old architecture (commit `efd42a4`, 2026-05-03), before starting
the rewrite. This fixes today's failure and prevents a future flip
from `master` to the new architecture from silently breaking the
build (or outright failing to compile, since the patches touch
internal classes like `Colorizer`, `Color`, `Language`).

If this breaks again later: check whether upstream moved the
`Legacy` tag (it shouldn't, it's a fixed release) or whether the
three patches need to be manually updated against the new state of
`TextEditor.cpp`/`.h` at that tag -- see the "Fix 2026-07: `Color`
moved..." section above for an example of how this was
diagnosed/fixed last time.

## imguicolortextedit_smart_backspace_pair.patch

`ImGuiColorTextEdit` (goossens fork) already auto-closes paired
glyphs -- `(`, `[`, `{`, `'`, `"` -- and already wraps a selection
with a pair when you type an opener over it (`completePairedGlyphs`,
on by default; Ava Studio also turns it on explicitly in
`editor_panel.cpp`'s `InitEditorPanel` via `SetCompletePairedGlyphs`).
So "auto-close brackets/quotes + surround selection" from
`ide_checklist.md` section 1 was already implemented and enabled --
the checklist entry marking it `[ ]` was wrong.

What was still missing: pressing Backspace right after an
auto-inserted empty pair (cursor sitting between `(` and `)`, or `"`
and `"`, with nothing typed in between) only deleted the opener,
leaving the stray closer behind -- unlike VS Code/JetBrains-style
"smart backspace", which removes both in one keystroke.

This patch:
- In `handleBackspace`, for a cursor with no selection, checks
  whether the glyph immediately to the left and the glyph
  immediately to the right of the cursor form a matching pair
  (`CodePoint::isMatchingPair`, already used elsewhere in the file).
  If they do (and `completePairedGlyphs` is on, and it's not a
  word-mode/Ctrl+Backspace delete), it extends the delete range to
  include the closer too, via `document.getRight`.
- Doesn't touch anything else: word-mode backspace, backspace with an
  active selection, and backspace next to non-matching or
  non-adjacent glyphs behave exactly as before.
- `document.getCodePoint` already returns
  `IM_UNICODE_CODEPOINT_INVALID` past the end of a line, so this is
  safe at start/end of line without an extra bounds check.

Note this triggers on *any* adjacent matching pair, not only ones the
editor itself auto-inserted (the library doesn't track provenance per
pair) -- same as VS Code's smart backspace, which does the same thing
by adjacency, not by history.

## Application order
`CMakeLists.txt` applies all seven patches in a single call to
`git apply` (it accepts multiple files and applies them in order on
the clean clone): `imguicolortextedit_interpolation.patch`,
`imguicolortextedit_bold_keywords.patch`,
`imguicolortextedit_doc_comment.patch`,
`imguicolortextedit_import_path.patch`,
`imguicolortextedit_interface_name.patch`,
`imguicolortextedit_variable_name.patch`, then
`imguicolortextedit_smart_backspace_pair.patch`. They don't overlap --
each touches a distinct part of the file (or, for four of them,
appends its own enum entry and palette lines right after the previous
patch's), and the last one only touches `handleBackspace`, which none
of the others go near -- but if another patch is ever added, keep the
order and test `git apply patch1 patch2 ... patchN` by hand on a
clean clone before pushing it (see AvaStudio.md).
