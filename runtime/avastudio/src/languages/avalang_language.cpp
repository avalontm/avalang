#include "languages/avalang_language.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "languages/lexer_utils.h"
#include "src/builtins/builtin_names.h"

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
    // Literal hex (0x.../0X...): mismo prefijo que el lexer de ANTLR
    // (AvaLang.g4) y el resaltador de syntax_highlight.cpp -- se consume
    // entero y se corta antes de la rama decimal/float de abajo, porque
    // un hex nunca lleva '.'.
    if (*i == '0') {
        TextEditor::Iterator afterZero = i;
        ++afterZero;
        if (afterZero < end && (*afterZero == 'x' || *afterZero == 'X')) {
            TextEditor::Iterator afterX = afterZero;
            ++afterX;
            TextEditor::Iterator j = afterX;
            auto isHexDigit = [](ImWchar c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            };
            while (j < end && isHexDigit(*j)) ++j;
            if (j > afterX) return j; // al menos un digito hex tras 0x/0X
        }
    }
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

// Namespaces del modulo nativo 'system' (runtime/avalang/src/builtins/
// system_module.cpp): System, System.Console, System.DateTime,
// System.Thread, System.Environment, System.IO.{File,Directory},
// System.Diagnostics.Process. A diferencia de las clases declaradas por
// el usuario (IsKnownClassName, alimentada por UpdateKnownClassNames a
// partir de `class X` visto en algun tab abierto), estos nombres nunca
// aparecen como `class X` en codigo AvaLang -- los registra el runtime
// via RegisterNativeModule/SetDictEntry, no el parser -- asi que
// KnownClassRefCounts() jamas los contiene y sin esta lista quedaban sin
// colorear (texto plano) en vez de recibir el mismo preprocessor/kSynClass
// verde-azulado que cualquier otra clase. Misma lista que
// tools/vscode/syntaxes/avalang.tmLanguage.json#stdlib-namespaces, para
// que AvaStudio y la extension de VS Code coincidan.
const std::unordered_set<std::string> kNativeNamespaces = {
    "System", "Console", "DateTime", "Thread", "Environment",
    "IO", "File", "Directory", "Diagnostics", "Process",
};

// Clases nativas inyectadas por el host en runtime, no por el usuario --
// hoy solo `Application` (ver ApplicationClassSource en
// runtime/avahost/src/native/native_app_host.cpp: compila un
// `class Application ... end` sintetico dentro de avanative.exe antes de
// correr el entry file, nunca aparece como texto `class Application` en
// ningun .ava del proyecto). Mismo problema que kNativeNamespaces arriba
// pero para IsKnownClassName en vez de namespaces con '.': sin esta lista,
// `app as Application` y el autocompletado no reconocen `Application` como
// tipo -- ver su uso en AvaLangTokenizer (coloreado de `as Tipo`) y en
// editor_panel.cpp::RebuildAutocompleteTrie (autocompletado).
const std::unordered_set<std::string> kNativeClassNames = {
    "Application",
};

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
    return KnownClassRefCounts().count(name) != 0 || kNativeClassNames.count(name) != 0;
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
            // (`import a as b`). A known class/interface type (project-
            // declared via `class X`/`interface X`, or a native one like
            // `Application` -- see kNativeClassNames above) gets colored
            // the same way `new X` already does below, so `x as Application`
            // and `new Application()` on the same line look consistent.
            // Anything else (an import alias, a builtin primitive like
            // int/float/bool/string, or a genuinely unknown name) is left
            // unhandled (returns `start`) so the engine's own keyword/
            // identifier tables color builtin type names via
            // lang.identifiers, and everything else stays plain default
            // text rather than risk mislabeling an import alias as a type.
            state_ = State::kIdle;
            if (IsKnownInterfaceName(word)) {
                color = TextEditor::Color::interfaceName;
                return word_end;
            }
            if (IsKnownClassName(word)) {
                color = TextEditor::Color::preprocessor;
                return word_end;
            }
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
        if (word == "extern") {
            pending_extern_ = true;
            return start;
        }
        if (word == "as" && pending_extern_) {
            pending_extern_ = false;
            color = TextEditor::Color::preprocessor;
            return word_end;
        }
        pending_extern_ = false;
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
            if (after_ws < end && *after_ws == '.' &&
                (IsKnownClassName(word) || kNativeNamespaces.count(word))) {
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
    bool pending_extern_ = false;
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

const std::unordered_set<std::string>& NativeClassNames() { return kNativeClassNames; }

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
#define AVA_LANG_IDENTIFIER(name, fn) #name,
            AVA_BUILTIN_GLOBALS(AVA_LANG_IDENTIFIER)
#undef AVA_LANG_IDENTIFIER
            "bool", "string", "list", "dict",
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
