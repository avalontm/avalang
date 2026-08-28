#include "languages/avalang_language.h"

namespace studio::languages {

namespace {

TextEditor::Iterator GetAvaLangIdentifier(TextEditor::Iterator start, TextEditor::Iterator end) {
    if (start < end && TextEditor::CodePoint::isXidStart(*start)) {
        ++start;
        while (start < end && TextEditor::CodePoint::isXidContinue(*start)) ++start;
    }
    return start;
}

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

}

const TextEditor::Language* AvaLang() {
    static TextEditor::Language language = [] {
        TextEditor::Language lang;
        lang.name = "AvaLang";
        lang.caseSensitive = true;

        lang.singleLineComment = "#";

        lang.docCommentPrefix = "##";

        lang.hasSingleQuotedStrings = true;
        lang.hasDoubleQuotedStrings = true;
        lang.stringEscape = '\\';

        lang.otherStringStart = "$\"";
        lang.otherStringEnd = "\"";

        lang.keywords = {
            "if", "then", "elif", "else", "end",
            "while", "for", "in",
            "func", "class", "base",
            "return", "break", "continue", "pass",
            "import", "as", "local", "extern",
            "raise", "try", "catch", "finally",
            "yield",
            "or", "and", "not",
            "static", "private",
            "async", "await",
            "select", "case", "to", "is",
        };

        lang.declarations = {"true", "false", "nil"};

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

}
