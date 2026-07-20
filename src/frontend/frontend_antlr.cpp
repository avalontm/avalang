#include "frontend.h"
#include "../ast/ast_builder.h"
#include "../compiler/compiler.h"

#include <antlr4-runtime.h>
#include "AvaLangLexer.h"
#include "AvaLangParser.h"

#include <algorithm>
#include <sstream>

namespace ava {

struct SourceError {
    size_t line;
    size_t column;
    std::string message;
};

class AvaLangErrorListener : public antlr4::BaseErrorListener {
public:
    std::vector<SourceError> errors;
    std::string source_name;
    std::string source_text;

    explicit AvaLangErrorListener(const std::string& name, const std::string& text)
        : source_name(name), source_text(text) {}

    void syntaxError(antlr4::Recognizer*, antlr4::Token* offendingSymbol,
                     size_t line, size_t charPositionInLine,
                     const std::string& msg, std::exception_ptr) override {
        SourceError err;
        err.line = line;
        err.column = charPositionInLine + 1;
        err.message = msg;
        errors.push_back(err);
    }
};

static std::string getLine(const std::string& text, size_t line_num) {
    std::istringstream iss(text);
    std::string line;
    size_t current = 1;
    while (std::getline(iss, line)) {
        if (current == line_num) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        current++;
    }
    return "";
}

static std::string formatError(const std::string& source_name, const SourceError& err,
                               const std::string& source_text) {
    std::ostringstream out;
    
    out << "error at " << source_name << ":" << err.line << ":" << err.column 
        << ": " << err.message << "\n";
    
    std::string line_content = getLine(source_text, err.line);
    if (!line_content.empty()) {
        out << "    " << err.line << " | " << line_content << "\n";
        
        size_t display_col = std::min(err.column, line_content.size() + 1);
        size_t indent = 5 + std::to_string(err.line).size();
        out << std::string(indent, ' ');
        
        for (size_t i = 1; i < display_col; ++i) {
            if (i < line_content.size() && (line_content[i-1] == '\t' || 
                (line_content[i-1] >= 0 && line_content[i-1] < 32))) {
                out << line_content[i-1];
            } else {
                out << ' ';
            }
        }
        
        out << "^";
        
        if (err.column <= line_content.size()) {
            std::string token_text = line_content.substr(err.column - 1);
            size_t token_end = token_text.find_first_of(" \t\n\r.,;:!?()[]{}");
            if (token_end == std::string::npos) token_end = token_text.size();
            for (size_t i = 1; i < token_end && i < 20; ++i) {
                out << "~";
            }
        }
        out << "\n";
    }
    
    return out.str();
}

std::shared_ptr<Proto> CompileSource(const std::string& source, const std::string& source_name) {
    fprintf(stderr, "[C++] CompileSource: start\n");
    std::string normalized_source = source;
    if (normalized_source.empty() || normalized_source.back() != '\n') {
        normalized_source += '\n';
    }
    
    fprintf(stderr, "[C++] CompileSource: creating input stream\n");
    antlr4::ANTLRInputStream input(normalized_source);
    fprintf(stderr, "[C++] CompileSource: creating lexer\n");
    AvaLangLexer lexer(&input);
    
    AvaLangErrorListener error_listener(source_name, normalized_source);
    lexer.addErrorListener(&error_listener);
    
    fprintf(stderr, "[C++] CompileSource: creating token stream\n");
    antlr4::CommonTokenStream tokens(&lexer);
    
    fprintf(stderr, "[C++] CompileSource: creating parser\n");
    AvaLangParser parser(&tokens);
    parser.addErrorListener(&error_listener);
    
    fprintf(stderr, "[C++] CompileSource: parsing chunk\n");
    auto* tree = parser.chunk();
    fprintf(stderr, "[C++] CompileSource: parsed chunk\n");
    fprintf(stderr, "[C++] CompileSource: checking for errors, %zu errors\n", error_listener.errors.size());

    if (!error_listener.errors.empty()) {
        std::ostringstream err_out;
        for (const auto& err : error_listener.errors) {
            err_out << formatError(source_name, err, normalized_source);
        }
        throw CompileError(err_out.str());
    }

    fprintf(stderr, "[C++] CompileSource: checking syntax errors\n");
    if (parser.getNumberOfSyntaxErrors() > 0) {
        throw CompileError("syntax error(s) in " + source_name);
    }
    
    fprintf(stderr, "[C++] CompileSource: creating AstBuilder\n");
    AstBuilder builder;
    try {
        fprintf(stderr, "[C++] CompileSource: visiting parse tree\n");
        auto any_chunk = tree->accept(&builder);
        fprintf(stderr, "[C++] CompileSource: visited, has_value=%d\n", any_chunk.has_value());
        if (!any_chunk.has_value()) {
            throw CompileError("AST building returned empty result");
        }
        fprintf(stderr, "[C++] CompileSource: casting to Chunk\n");
        auto chunk = std::any_cast<std::shared_ptr<Chunk>>(any_chunk);
        fprintf(stderr, "[C++] CompileSource: chunk=%p\n", chunk.get());
        if (!chunk) {
            throw CompileError("AST chunk is null");
        }
        fprintf(stderr, "[C++] CompileSource: creating Compiler\n");
        Compiler compiler;
        fprintf(stderr, "[C++] CompileSource: chunk has %zu statements\n", chunk->statements.size());
        fprintf(stderr, "[C++] CompileSource: compiling chunk\n");
        auto proto = compiler.Compile(chunk);
        fprintf(stderr, "[C++] CompileSource: compiled proto=%p\n", proto.get());
        return proto;
    } catch (const std::bad_any_cast& e) {
        throw CompileError("Bad any_cast during compilation: " + std::string(e.what()));
    } catch (const std::exception& e) {
        fprintf(stderr, "[C++] CompileSource: exception: %s\n", e.what());
        throw CompileError("Compilation error: " + std::string(e.what()));
    }
}

} // namespace ava