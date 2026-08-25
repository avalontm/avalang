#pragma once

#include <string>
#include <vector>

#include "imgui.h"

namespace studio::syntax {

enum class TokenKind {
    Default,
    Keyword,
    Function,
    Type,
    String,
    Number,
    Comment,
};

struct Token {
    int start;
    int end;
    TokenKind kind;
};

std::vector<Token> Tokenize(const std::string& text);

ImU32 ColorForToken(TokenKind kind);

std::vector<std::string> CollectIdentifiers(const std::string& text);

extern const std::vector<std::string> kKeywords;
extern const std::vector<std::string> kTypes;
extern const std::vector<std::string> kBuiltinFunctions;

}
