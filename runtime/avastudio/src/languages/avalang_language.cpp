#include "languages/avalang_language.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "languages/lexer_utils.h"

namespace studio::languages {

namespace {

TextEditor::Iterator GetAvaLangIdentifier(TextEditor::Iterator start, TextEditor::Iterator end) {
    if (start < end && TextEditor::CodePoint::isXidStart(*start)) {
        ++start;
        while (start < end && TextEditor::CodePoint::isXidContinue(*start)) ++start;
    }
    return start;
}

bool MatchesKeywordAt(TextEditor::Iterator pos, TextEditor::Iterator end, const char* keyword) {
    TextEditor::Iterator it = pos;
    for (const char* c = keyword; *c; ++c, ++it) {
        if (it >= end || *it != static_cast<ImWchar>(*c)) return false;
    }
    if (it < end && TextEditor::CodePoint::isXidContinue(*it)) return false;
    return true;
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
    "func", "class", "interface", "base", "new", "override", "this",
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

// Shared across every tab (see the header comment on UpdateKnownInterfaceNames).
// Value is the number of currently-open tabs that currently declare that
// name as an interface; a name is "known" while its count is > 0.
std::unordered_map<std::string, int>& KnownInterfaceRefCounts() {
    static std::unordered_map<std::string, int> counts;
    return counts;
}

bool IsKnownInterfaceName(const std::string& name) {
    return KnownInterfaceRefCounts().count(name) != 0;
}

// See the header comment on KnownInterfaceNamesGeneration().
int& KnownInterfaceGeneration() {
    static int generation = 0;
    return generation;
}

std::unordered_map<std::string, int>& KnownVariableRefCounts() {
    static std::unordered_map<std::string, int> counts;
    return counts;
}

bool IsKnownVariableName(const std::string& name) {
    return KnownVariableRefCounts().count(name) != 0;
}

int& KnownVariableGeneration() {
    static int generation = 0;
    return generation;
}

std::unordered_map<std::string, int>& KnownClassRefCounts() {
    static std::unordered_map<std::string, int> counts;
    return counts;
}

bool IsKnownClassName(const std::string& name) {
    return KnownClassRefCounts().count(name) != 0;
}

int& KnownClassGeneration() {
    static int generation = 0;
    return generation;
}

class AvaLangTokenizer {
public:
    TextEditor::Iterator operator()(TextEditor::Iterator start, TextEditor::Iterator end, TextEditor::Color& color) {
        TextEditor::Iterator word_end = GetAvaLangIdentifier(start, end);
        if (word_end == start) {
            if (start < end && *start == '(') {
                ++paren_depth_;
            } else if (start < end && *start == ')' && paren_depth_ > 0) {
                --paren_depth_;
            }
            if (state_ == State::kExpectColonOrBody && start < end && *start == ':') {
                state_ = State::kExpectBaseName;
            } else if (state_ == State::kExpectMoreBase && start < end && *start == ',') {
                state_ = State::kExpectBaseName;
            } else if (state_ == State::kExpectMoreImportPath && start < end && *start == '.') {
                // Dotted continuation: `import a.b.c` -- loop back to
                // expect the next NAME segment. The '.' itself is left
                // unhandled (returns `start` below) so the engine's
                // default punctuation coloring applies to it, same as
                // the ',' between base names above.
                state_ = State::kExpectImportPath;
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
            color = declaring_interface_ ? TextEditor::Color::interfaceName : TextEditor::Color::preprocessor;
            return word_end;
        }
        if (state_ == State::kExpectBaseName) {
            state_ = State::kExpectMoreBase;
            color = IsKnownInterfaceName(word) ? TextEditor::Color::interfaceName
                                                : TextEditor::Color::preprocessor;
            return word_end;
        }
        if (state_ == State::kExpectImportPath) {
            // Each NAME segment of `import a.b.c` (see kExpectMoreImportPath
            // above for the dotted continuation). Unlike class/interface
            // names, this needs no known-name lookup -- every segment gets
            // the same color regardless of whether it resolves.
            state_ = State::kExpectMoreImportPath;
            color = TextEditor::Color::importPath;
            return word_end;
        }
        if (state_ == State::kExpectTypeName) {
            // The NAME right after `as` -- either a typeAnnotation's type
            // (`x as int`, `x as int = 1`) or an import alias
            // (`import a as b`). Left unhandled (returns `start` below) on
            // purpose: it must NOT go through the assignment/`as`/`in`
            // checks further down, since a typed assignment's type name
            // sits directly before the `=` too (`NAME 'as' NAME '=' expr`)
            // and would otherwise get wrongly colored as the assignment
            // target itself. Falling through lets the engine's own
            // keyword/identifier tables color builtin type names
            // (int/float/bool/string, via lang.identifiers) normally, and
            // leaves anything else (a class/interface type, an import
            // alias) as plain default text.
            state_ = State::kIdle;
            return start;
        }
        state_ = State::kIdle;

        if (pending_new_) {
            pending_new_ = false;
            // `new` always instantiates a class -- interfaces can't be
            // instantiated -- so this is unambiguous without a lookup.
            color = TextEditor::Color::preprocessor;
            return word_end;
        }
        pending_new_ = false;

        if (word == "class" || word == "interface") {
            declaring_interface_ = (word == "interface");
            state_ = State::kExpectClassName;
            return start;
        }
        if (word == "new") {
            pending_new_ = true;
            return start;
        }
        if (word == "import") {
            // `import a.b.c as d` -- the grammar is
            // 'import' NAME ('.' NAME)* ('as' NAME)?  (see AvaLang.g4).
            // Leave `import` itself unhandled (returns `start` below) so
            // it still gets the normal keyword color from the engine's
            // default identifier path; only the NAME segments that
            // follow get colored here (kExpectImportPath above). The
            // optional trailing `as NAME` goes through kExpectTypeName
            // below like every other `as` (see the `word == "as"` check).
            state_ = State::kExpectImportPath;
            return start;
        }
        if (word == "as") {
            state_ = State::kExpectTypeName;
            return start;
        }

        TextEditor::Iterator after_ws = word_end;
        while (after_ws < end && (*after_ws == ' ' || *after_ws == '\t')) ++after_ws;
        if (after_ws < end && *after_ws == '(' && !kNonFunctionWords.count(word)) {
            color = TextEditor::Color::declaration;
            return word_end;
        }

        if (!kNonFunctionWords.count(word)) {
            if (paren_depth_ == 0 && after_ws < end) {
                ImWchar c0 = *after_ws;
                TextEditor::Iterator c1 = after_ws;
                ++c1;
                bool isAssign = false;
                if (c0 == '=') {
                    isAssign = !(c1 < end && (*c1 == '=' || *c1 == '>'));
                } else if ((c0 == '+' || c0 == '-' || c0 == '*' || c0 == '%') &&
                           c1 < end && *c1 == '=') {
                    isAssign = true;
                } else if (c0 == '/' && c1 < end && *c1 == '=') {
                    isAssign = true;
                } else if (c0 == '/' && c1 < end && *c1 == '/') {
                    TextEditor::Iterator c2 = c1;
                    ++c2;
                    isAssign = c2 < end && *c2 == '=';
                }
                if (isAssign) {
                    color = TextEditor::Color::variableName;
                    return word_end;
                }
            }
            if (MatchesKeywordAt(after_ws, end, "as") || MatchesKeywordAt(after_ws, end, "in")) {
                color = TextEditor::Color::variableName;
                return word_end;
            }
            if (IsKnownVariableName(word)) {
                color = TextEditor::Color::variableName;
                return word_end;
            }
            if (after_ws < end && *after_ws == '.' && IsKnownClassName(word)) {
                color = TextEditor::Color::preprocessor;
                return word_end;
            }
        }

        return start;
    }

private:
    enum class State {
        kIdle, kExpectClassName, kExpectColonOrBody, kExpectBaseName, kExpectMoreBase,
        kExpectImportPath, kExpectMoreImportPath, kExpectTypeName,
    };

    State state_ = State::kIdle;
    bool declaring_interface_ = false;
    bool pending_new_ = false;
    int paren_depth_ = 0;
};

}

void UpdateKnownInterfaceNames(const std::unordered_set<std::string>& removed,
                                const std::unordered_set<std::string>& added) {
    auto& counts = KnownInterfaceRefCounts();
    bool changed = false;
    for (const auto& name : removed) {
        auto it = counts.find(name);
        if (it == counts.end()) continue;  // already released (or never added) -- ignore
        if (--it->second <= 0) {
            counts.erase(it);
            changed = true;  // name just became unknown
        }
    }
    for (const auto& name : added) {
        if (++counts[name] == 1) changed = true;  // name just became known
    }
    if (changed) ++KnownInterfaceGeneration();
}

int KnownInterfaceNamesGeneration() { return KnownInterfaceGeneration(); }

void UpdateKnownVariableNames(const std::unordered_set<std::string>& removed,
                               const std::unordered_set<std::string>& added) {
    auto& counts = KnownVariableRefCounts();
    bool changed = false;
    for (const auto& name : removed) {
        auto it = counts.find(name);
        if (it == counts.end()) continue;
        if (--it->second <= 0) {
            counts.erase(it);
            changed = true;
        }
    }
    for (const auto& name : added) {
        if (++counts[name] == 1) changed = true;
    }
    if (changed) ++KnownVariableGeneration();
}

int KnownVariableNamesGeneration() { return KnownVariableGeneration(); }

void UpdateKnownClassNames(const std::unordered_set<std::string>& removed,
                            const std::unordered_set<std::string>& added) {
    auto& counts = KnownClassRefCounts();
    bool changed = false;
    for (const auto& name : removed) {
        auto it = counts.find(name);
        if (it == counts.end()) continue;
        if (--it->second <= 0) {
            counts.erase(it);
            changed = true;
        }
    }
    for (const auto& name : added) {
        if (++counts[name] == 1) changed = true;
    }
    if (changed) ++KnownClassGeneration();
}

int KnownClassNamesGeneration() { return KnownClassGeneration(); }

std::unordered_set<std::string> ScanKnownVariableNames(const std::string& text) {
    using lexer::IsIdentChar;
    using lexer::IsIdentStart;
    using lexer::ReadIdent;
    using lexer::SkipInlineWhitespace;

    std::unordered_set<std::string> names;
    const size_t end = text.size();
    size_t i = 0;
    int paren_depth = 0;
    bool skip_next_word = false;

    while (i < end) {
        char c = text[i];

        if (c == '#') { while (i < end && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < end && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < end) i += 2; else ++i;
            }
            if (i < end) ++i;
            continue;
        }
        if (c == '(') { ++paren_depth; ++i; continue; }
        if (c == ')') { if (paren_depth > 0) --paren_depth; ++i; continue; }
        if (!IsIdentStart(c)) { ++i; continue; }

        std::string word = ReadIdent(text, i);
        if (skip_next_word) {
            skip_next_word = false;
            continue;
        }
        if (word == "as") {
            skip_next_word = true;
            continue;
        }
        if (kNonFunctionWords.count(word)) continue;

        size_t after_ws = i;
        SkipInlineWhitespace(text, after_ws);

        bool matched = false;
        if (paren_depth == 0 && after_ws < end) {
            char c0 = text[after_ws];
            char c1 = after_ws + 1 < end ? text[after_ws + 1] : '\0';
            if (c0 == '=' && c1 != '=' && c1 != '>') {
                matched = true;
            } else if ((c0 == '+' || c0 == '-' || c0 == '*' || c0 == '%') && c1 == '=') {
                matched = true;
            } else if (c0 == '/' && c1 == '=') {
                matched = true;
            } else if (c0 == '/' && c1 == '/') {
                char c2 = after_ws + 2 < end ? text[after_ws + 2] : '\0';
                matched = c2 == '=';
            }
        }
        if (!matched && after_ws + 1 < end && text[after_ws] == 'a' && text[after_ws + 1] == 's' &&
            (after_ws + 2 >= end || !IsIdentChar(text[after_ws + 2]))) {
            matched = true;
        }
        if (!matched && after_ws + 1 < end && text[after_ws] == 'i' && text[after_ws + 1] == 'n' &&
            (after_ws + 2 >= end || !IsIdentChar(text[after_ws + 2]))) {
            matched = true;
        }
        if (matched) names.insert(word);
    }
    return names;
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
            "func", "class", "interface", "base", "new", "override", "this",
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
            "print", "type", "typeof", "str", "int", "float", "bool", "string", "list", "dict",
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
