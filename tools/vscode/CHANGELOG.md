# Changelog

## 0.1.2

- Dictionary keys now highlight like JSON keys, both bare (`{key: v}`) and quoted (`{"key": v}`).
- Property access (`this._name`, `obj.attr`) and named call arguments (`foo(x = 1)`) get their own
  scopes instead of falling back to plain identifiers.
- Line continuation (`\` at end of line) is highlighted.
- Marker-based code folding (`if/while/for/func/class/try/extern` ... `end`) instead of relying
  only on indentation.
- Comments auto-continue with `# ` when pressing Enter inside a comment line.
- Default `.ava` editor settings (tabSize 4, no indentation auto-detection).
- Added GitHub Actions workflow to package the `.vsix` on every push and publish on tag `v*`.
- Verified the grammar with an automated tokenizer check (vscode-textmate) confirming no unclosed
  string/dict/extern regions and correct scopes for the new rules.

## 0.1.1

- Added language snippets (`snippets/avalang.json`) for `if`, `if/else`, `if/elif/else`, `while`,
  `for/in`, `func`, `class`, `class` with heritage, `try/catch`, `try/catch/finally`, `extern`,
  short lambdas and f-strings, all using AvaLang's real `then ... end` block syntax instead of
  curly braces.

## 0.1.0

- Resaltado de sintaxis inicial para AvaLang, basado en `runtime/avalang/grammar/AvaLang.g4`.
- Soporte para clases, herencia, `extern` FFI, f-strings interpoladas, lambdas cortas y todos los
  operadores/palabras clave de la gramática.
- Configuración de lenguaje con auto-cierre de brackets/comillas e indentación básica.
