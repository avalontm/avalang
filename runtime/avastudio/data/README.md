# `data/` — Ava Studio editable content

```
data/
  docs/     keyword_docs.csv, builtin_signatures.csv  (editor tooltips/autocomplete)
  design/   component_catalog.csv                     (Toolbox components)
  langs/    en.csv, es.csv                             (UI strings, see util/i18n.h)
```

## `docs/` — tooltips and autocomplete

These two files are what the Code Editor shows in its tooltips
(`DrawKeywordHint` / `DrawParameterHint` in `panels/editor_panel.cpp`) and
what feeds the autocomplete popup. They're edited with any plain text
editor — **there's no need to recompile Ava Studio**: changes show up
the next time you open the app, or right away if you switch back to
Ava Studio after saving the file (the modification date is checked
every time a tooltip needs to be shown).

If either file doesn't exist or has a formatting error that prevents
reading it, Ava Studio falls back to a backup table embedded in the
executable (the same content these CSVs ship with by default) — it
never ends up without tooltips, it just stops seeing your changes
until the file is fixed.

**Important:** neither `grammar/AvaLang.g4` nor the compiler reads
these files, and these files don't generate anything in the grammar
either. If you add a new keyword or function to the language, you
have to add the row here BY HAND for the editor to know about it —
these are two separate things that aren't connected today.

### `docs/keyword_docs.csv` — keywords (`if`, `while`, `try`, ...)

Columns: `name,syntax,example,doc`

| Column | What it is | Required |
|---|---|---|
| `name` | The keyword as-is (`if`, `while`, ...) | yes |
| `syntax` | The abstract usage pattern, with placeholder names like `condition` | yes |
| `example` | A concrete, copy-pasteable example, with real values instead of placeholder names | no, can be left empty |
| `doc` | One or two sentences explaining what it does | yes |

Inside a cell:
- `\n` (backslash + n, two characters) is a line break. Real line
  breaks are not used inside a cell.
- If the keyword accepts more than one way of being written (e.g.
  `while` with or without parentheses), both are put separated by
  `|||` (three vertical bars) within the same `syntax` cell.
- If a cell has commas, quotes, or needs to be made unambiguous,
  wrap it in double quotes (`"..."`); a literal double quote inside
  is written doubled (`""`).

Example of a full row:

```csv
while,"while condition\n    ...\nend|||while (condition)\n    ...\nend","count = 0\nwhile count < 5\n    print(count)\n    count = count + 1\nend","Repeats the block for as long as condition stays true."
```

Leaving `example` empty makes sense for keywords that are only
understood alongside another one (`then`, `in`, `as`, `catch`) — the
example for the main keyword (`if`, `for`, `import`, `try`) already
covers them.

### `docs/builtin_signatures.csv` — built-in functions (`print`, `len`, `range`, ...)

Columns: `name,params,doc`

| Column | What it is | Required |
|---|---|---|
| `name` | The function name | yes |
| `params` | List of parameters separated by `\|` (a single bar) | yes, can be an empty list if it takes nothing |
| `doc` | What it does, and how to call it if it has more than one form | yes |

A parameter starting with `*` (e.g. `*values`) means "accepts zero
or more arguments" — no separate column is needed for that, the
asterisk in the parameter name is enough, same as in the real
signature.

Example:

```csv
pow,base|exponent,Returns base raised to exponent (base ** exponent).
print,*values,"Prints every argument, converted with the same rules as str()..."
```

## `design/component_catalog.csv` — Toolbox components (`Button`, `TextBox`, `CheckBox`, ...)

Columns: `type,display_name,is_container,properties`

| Column | What it is | Required |
|---|---|---|
| `type` | The type as used by AvaComponent (`button`, `stack`, ...) | yes |
| `display_name` | The label shown in the Toolbox (`Button`, `Stack`, ...) | yes |
| `is_container` | `true` or `false` -- whether it accepts other components being dropped inside | yes |
| `properties` | Default properties for a new instance, as `key=default` separated by `\|` (a single bar) | no, can be left empty (containers like `column`/`row`/`stack`/`flex`) |

Example of a full row:

```csv
button,Button,false,value=Button|enabled=true
```

Today's default values are simple words, booleans, or empty
strings -- if a default ever needed to literally contain `|` or `=`,
this column doesn't support that yet (no escaping is defined for
it).

If a `.avaui` references a `type` that isn't in this CSV (for
example, from a newer version of Ava Studio), `FindComponentType`
returns `nullptr` and the Designer Canvas falls back to plain text
instead of failing -- see `panels/designer_canvas.cpp`.

## How to add a new keyword or function

1. Add it first wherever it belongs in the actual language: the
   grammar (`grammar/AvaLang.g4`) for a keyword, or
   `core/src/builtins/builtin_natives.h/.cpp` + `RegisterBuiltinGlobals`
   for a function.
2. Also add it to `studio/src/languages/avalang_language.cpp`
   (`lang.keywords` or `lang.identifiers`) so it gets highlighted and
   shows up in autocomplete — this step is still C++, it's not in
   the CSV.
3. Add the corresponding row here (`docs/keyword_docs.csv` or
   `docs/builtin_signatures.csv`) so it has a tooltip. This step really is
   just text, no recompiling needed.

If a generator from the grammar is ever built
(`tools/dump_docs.cpp` already exists to dump the factory table to
these CSVs, but it doesn't read the grammar yet), step 3 would stop
being manual for keywords. See `autocompletado-avalang.md` for the
full context on that idea.

## `langs/` — UI strings (i18n)

Columns: `key,value`

One file per locale: `en.csv` (canonical -- every key must exist here) and `es.csv`. A key missing from `es.csv` falls back to `en.csv`; missing from both shows up in the UI as `[key]` on purpose, so an untranslated string is easy to spot during development. See `util/i18n.h`/`Tr()`. Re-checked for changes the same way as the other CSVs here -- no recompile needed to see an edit.
