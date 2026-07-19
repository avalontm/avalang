// Built against the antlr4-runtime C++ API (antlr4::Token, CommonToken,
// TokenSource). Only compiled when AVA_HAVE_ANTLR is defined by CMake
// (i.e. the antlr4-runtime package was found) -- see CMakeLists.txt.
#include "denter.h"

#include <antlr4-runtime.h>

namespace ava {

Denter::Denter(antlr4::TokenSource* source,
               size_t newline_token_type,
               size_t indent_token_type,
               size_t dedent_token_type,
               size_t eof_token_type)
    : source_(source),
      newline_type_(newline_token_type),
      indent_type_(indent_token_type),
      dedent_type_(dedent_token_type),
      eof_type_(eof_token_type) {}

size_t Denter::MeasureIndent(const std::string& whitespace_tail) {
    // Tabs count as one column here for simplicity; if mixed tab/space
    // indentation needs to be an error (like real Python does), that
    // validation belongs here too.
    size_t width = 0;
    for (char c : whitespace_tail) {
        if (c == ' ' || c == '\t') width++;
    }
    return width;
}

std::unique_ptr<antlr4::Token> Denter::NextToken() {
    if (!pending_.empty()) {
        auto tok = std::move(pending_.front());
        pending_.erase(pending_.begin());
        return tok;
    }

    std::unique_ptr<antlr4::Token> raw(source_->nextToken());

    if (raw->getType() == static_cast<size_t>(antlr4::Token::EOF)) {
        // Flush remaining DEDENTs so every INDENT is balanced before EOF.
        if (!emitted_final_dedents_) {
            emitted_final_dedents_ = true;
            while (indent_stack_.size() > 1) {
                indent_stack_.pop_back();
                auto dedent = std::make_unique<antlr4::CommonToken>(dedent_type_, "");
                pending_.push_back(std::move(dedent));
            }
            pending_.push_back(std::move(raw));
            return NextToken();
        }
        return raw;
    }

    if (raw->getType() != newline_type_) {
        return raw;
    }

    // NEWLINE's text (per the grammar) is the line break plus the leading
    // whitespace of the next line, so we can measure the new indent width
    // directly from it without re-lexing.
    const std::string& text = raw->getText();
    size_t last_break = text.find_last_of("\n");
    std::string tail = (last_break == std::string::npos) ? "" : text.substr(last_break + 1);
    size_t new_width = MeasureIndent(tail);

    pending_.push_back(std::move(raw)); // keep the NEWLINE itself

    if (new_width > indent_stack_.back()) {
        indent_stack_.push_back(new_width);
        pending_.push_back(std::make_unique<antlr4::CommonToken>(indent_type_, ""));
    } else {
        while (new_width < indent_stack_.back()) {
            indent_stack_.pop_back();
            pending_.push_back(std::make_unique<antlr4::CommonToken>(dedent_type_, ""));
        }
        // Mismatched indentation width (new_width doesn't match any level
        // on the stack) is a real syntax error in Python-like languages;
        // surfacing that cleanly is a TODO for the error-reporting pass.
    }

    auto tok = std::move(pending_.front());
    pending_.erase(pending_.begin());
    return tok;
}

} // namespace ava
