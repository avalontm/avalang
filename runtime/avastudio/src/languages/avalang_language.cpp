#include "languages/avalang_language.h"

#include <string>
#include <unordered_set>

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

const std::unordered_set<std::string> kNonFunctionWords = {
    "if", "then", "elif", "else", "end",
    "while", "for", "in",
    "func", "class", "base", "new", "override", "this",
    "return", "break", "continue", "pass",
    "import", "as", "local", "extern",
    "raise", "try", "catch", "finally",
    "yield",
    "or", "and", "not",
    "static", "private",
    "async", "await",
    "select", "case", "to", "is",
    "true", "false", "nil",
};

class AvaLangTokenizer {
public:
    TextEditor::Iterator operator()(TextEditor::Iterator start, TextEditor::Iterator end, TextEditor::Color& color) {
        TextEditor::Iterator word_end = GetAvaLangIdentifier(start, end);
        if (word_end == start) {
            if (state_ == State::kExpectColonOrBody && start < end && *start == ':') {
                state_ = State::kExpectBaseName;
            } else {
                state_ = State::kIdle;
            }
            pending_new_ = false;
            return start;
        }

        std::string word;
        for (auto it = start; it != word_end; ++it) word.push_back(static_cast<char>(*it));

        if (state_ == State::kExpectClassName) {
            state_ = State::kExpectColonOrBody;
            color = TextEditor::Color::preprocessor;
            return word_end;
        }
        if (state_ == State::kExpectBaseName) {
            state_ = State::kIdle;
            color = TextEditor::Color::preprocessor;
            return word_end;
        }
        state_ = State::kIdle;

        if (pending_new_) {
            pending_new_ = false;
            color = TextEditor::Color::preprocessor;
            return word_end;
        }
        pending_new_ = false;

        if (word == "class") {
            state_ = State::kExpectClassName;
            return start;
        }
        if (word == "new") {
            pending_new_ = true;
            return start;
        }

        TextEditor::Iterator after_ws = word_end;
        while (after_ws < end && (*after_ws == ' ' || *after_ws == '\t')) ++after_ws;
        if (after_ws < end && *after_ws == '(' && !kNonFunctionWords.count(word)) {
            color = TextEditor::Color::declaration;
            return word_end;
        }

        return start;
    }

private:
    enum class State { kIdle, kExpectClassName, kExpectColonOrBody, kExpectBaseName };

    State state_ = State::kIdle;
    bool pending_new_ = false;
};

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
            "func", "class", "base", "new", "override", "this",
            "return", "break", "continue", "pass",
            "import", "as", "local", "extern",
            "raise", "try", "catch", "finally",
            "yield",
            "or", "and", "not",
            "static", "private",
            "async", "await",
            "select", "case", "to", "is",
            "true", "false", "nil",
        };

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
        lang.customTokenizer = AvaLangTokenizer{};

        return lang;
    }();

    return &language;
}

}
