#pragma once

#include "TextEditor.h"

namespace studio::languages {

// AvaLang's syntax rules for the Code Editor panel, mirroring the ANTLR
// grammar in grammar/AvaLang.g4: `then/do/end` block delimiters, `#`
// comments, single/double quoted strings (including f-strings), and the
// keyword set from the grammar's lexer rules.
//
// Kept in its own translation unit (rather than inline in editor_panel.cpp)
// so it's easy to find and update if the grammar changes.
const TextEditor::Language* AvaLang();

} // namespace studio::languages
