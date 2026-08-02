#ifndef AVA_FRONTEND_DENTER_H
#define AVA_FRONTEND_DENTER_H

#include <memory>
#include <vector>
#include <stdexcept>

// Forward declarations to avoid pulling the full ANTLR4 runtime headers
// into every translation unit that includes this file.
namespace antlr4 { class Token; class TokenSource; }

namespace ava {

// Wraps a token source (the generated AvaLangLexer) and rewrites its
// NEWLINE tokens (which, per the grammar, greedily consume the leading
// whitespace of the following line) into a NEWLINE followed by zero or
// more INDENT/DEDENT tokens, by tracking an indentation-width stack.
//
// This is the standard technique for layering Python-like significant
// whitespace on top of ANTLR4, which has no native support for it (see
// AvaLang.g4 header comment and DESIGN.md section 1).
class Denter {
public:
    explicit Denter(antlr4::TokenSource* source,
                     size_t newline_token_type,
                     size_t indent_token_type,
                     size_t dedent_token_type,
                     size_t eof_token_type);

    // Returns the next token, transparently injecting INDENT/DEDENT as
    // needed. Intended to be driven by a custom antlr4::TokenSource
    // adapter (see frontend.cpp) that ANTLR4's CommonTokenStream consumes
    // in place of the raw lexer.
    std::unique_ptr<antlr4::Token> NextToken();

private:
    antlr4::TokenSource* source_;
    size_t newline_type_;
    size_t indent_type_;
    size_t dedent_type_;
    size_t eof_type_;

    std::vector<size_t> indent_stack_{0};
    std::vector<std::unique_ptr<antlr4::Token>> pending_;
    bool at_line_start_ = true;
    bool emitted_final_dedents_ = false;

    static size_t MeasureIndent(const std::string& whitespace_tail);
};

} // namespace ava

#endif // AVA_FRONTEND_DENTER_H
