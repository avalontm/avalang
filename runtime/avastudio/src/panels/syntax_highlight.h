#pragma once

#include <string>
#include <vector>

#include "imgui.h"

// Hand-written, best-effort lexer for AvaLang (mirrors grammar/AvaLang.g4).
// It is not a full parser -- it exists purely to classify spans of source
// text for the editor's syntax-highlighting overlay and for autocomplete
// candidate collection, the same way a TextMate/VSCode grammar would.
namespace studio::syntax {

enum class TokenKind {
    Default, // punctuation / operators / whitespace -- same color as Variable
    Keyword,
    Function,
    Type,
    String,
    Number,
    Comment,
};

struct Token {
    int start; // inclusive, byte offset into the source buffer
    int end;   // exclusive
    TokenKind kind;
};

// Tokenizes the whole buffer into contiguous spans (no gaps) covering
// every byte of `text`.
std::vector<Token> Tokenize(const std::string& text);

// Maps a TokenKind to its brand color (see palette.h).
ImU32 ColorForToken(TokenKind kind);

// Every identifier (NAME) that appears anywhere in the buffer, deduped,
// excluding language keywords. Used to offer user-defined function and
// variable names in autocomplete.
std::vector<std::string> CollectIdentifiers(const std::string& text);

extern const std::vector<std::string> kKeywords;
extern const std::vector<std::string> kTypes;
extern const std::vector<std::string> kBuiltinFunctions;

} // namespace studio::syntax
