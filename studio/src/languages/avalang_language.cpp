#include "languages/avalang_language.h"

namespace studio::languages {

namespace {

// grammar/AvaLang.g4: NAME : [a-zA-Z_] [a-zA-Z_0-9]*
TextEditor::Iterator GetAvaLangIdentifier(TextEditor::Iterator start, TextEditor::Iterator end) {
    if (start < end && TextEditor::CodePoint::isXidStart(*start)) {
        ++start;
        while (start < end && TextEditor::CodePoint::isXidContinue(*start)) ++start;
    }
    return start;
}

// grammar/AvaLang.g4: NUMBER : DIGIT+ ('.' DIGIT+)?
TextEditor::Iterator GetAvaLangNumber(TextEditor::Iterator start, TextEditor::Iterator end) {
    TextEditor::Iterator i = start;
    if (i >= end || *i < '0' || *i > '9') return start;
    while (i < end && *i >= '0' && *i <= '9') ++i;
    if (i < end && *i == '.') {
        TextEditor::Iterator afterDot = i;
        ++afterDot;
        if (afterDot < end && *afterDot >= '0' && *afterDot <= '9') {
            i = afterDot;
            while (i < end && *i >= '0' && *i <= '9') ++i;
        }
    }
    return i;
}

} // namespace

const TextEditor::Language* AvaLang() {
    static TextEditor::Language language = [] {
        TextEditor::Language lang;
        lang.name = "AvaLang";
        lang.caseSensitive = true;

        // grammar/AvaLang.g4: `COMMENT : '#' ~[\r\n]* -> skip` -- no
        // multiline comment syntax exists.
        lang.singleLineComment = "#";

        // "##" doc-comment blocks (used for function/param docs, see
        // function_index.cpp) get their own color, checked before the
        // plain "#" comment above.
        lang.docCommentPrefix = "##";

        // grammar/AvaLang.g4: STRING accepts both quote styles, FSTRING
        // ($"...") is the interpolated variant; both use the same escapes.
        lang.hasSingleQuotedStrings = true;
        lang.hasDoubleQuotedStrings = true;
        lang.stringEscape = '\\';

        // FSTRING goes through the "otherString" mechanism instead of the
        // plain double-quote flag above: that's the one that lets the
        // colorizer track `{expr}` interpolation nesting and give it its
        // own color (Color::interpolation, see patches/) instead of
        // painting the whole f-string in a single flat string color.
        lang.otherStringStart = "$\"";
        lang.otherStringEnd = "\"";

        // Statement/control-flow keywords (smallStatement, compoundStatement,
        // primary rules in the grammar).
        lang.keywords = {
            "if", "then", "elif", "else", "end",
            "while", "for", "in",
            "func", "class", "base",
            "return", "break", "continue", "pass",
            "import", "as", "local",
            "raise", "try", "catch", "finally",
            "yield",
            "or", "and", "not",
        };

        // Literal keywords -- colored distinctly from control-flow keywords.
        lang.declarations = {"true", "false", "nil"};

        // Built-in native functions registered in
        // core/src/builtins/builtin_init.cpp -- colored as "known
        // identifiers" so they read differently from user-defined names.
        lang.identifiers = {
            "print", "type", "str", "int", "float",
            "abs", "round", "floor", "ceil", "min", "max", "pow", "sqrt", "sum",
            "sorted", "reversed", "any", "all", "len", "range",
        };

        lang.isPunctuation = [](ImWchar ch) {
            switch (ch) {
                case '+': case '-': case '*': case '/': case '%':
                case '(': case ')': case '[': case ']': case '{': case '}':
                case '.': case ',': case ':': case ';':
                case '=': case '<': case '>': case '!':
                case '&': case '|': case '^': case '~':
                case '$':
                    return true;
                default:
                    return false;
            }
        };

        lang.getIdentifier = GetAvaLangIdentifier;
        lang.getNumber = GetAvaLangNumber;

        return lang;
    }();

    return &language;
}

} // namespace studio::languages
