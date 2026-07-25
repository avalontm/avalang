#pragma once

#include "TextEditor.h"

namespace studio {

inline TextEditor::LanguageDefinition AvaLangLanguageDef() {
    TextEditor::LanguageDefinition lang;
    lang.mName = "AvaLang";
    
    // Single-line comments: #
    lang.mSingleLineComment = "#";
    
    // Keywords basados en la gramática AvaLang.g4
    static const char* const keywords[] = {
        // Control flow
        "if", "elif", "else", "while", "for", "then", "end",
        // Functions and classes
        "func", "class", "return", "base",
        // Variable declarations
        "local",
        // Control statements
        "break", "continue", "pass", "raise", "yield",
        // Exception handling
        "try", "catch", "finally",
        // Imports
        "import", "as",
        // Literals
        "true", "false", "nil",
        // Operators
        "and", "or", "not"
    };
    
    for (auto& kw : keywords) {
        lang.mKeywords.insert(kw);
    }
    
// --- Strings (PRIMERO para que se capturen antes que identifiers) ---
    // Double-quoted strings: "..."
    lang.mTokenRegexStrings.push_back(std::make_pair("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", TextEditor::PaletteIndex::String));
    
    // Single-quoted strings: '...'
    lang.mTokenRegexStrings.push_back(std::make_pair("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'", TextEditor::PaletteIndex::String));
    
    // F-strings: $"..." con interpolación {expresiones}
    lang.mTokenRegexStrings.push_back(std::make_pair(R"(\$"[^"\$]*(?:\$[^"{]|\{[^}]*\})*[^\"]*")", TextEditor::PaletteIndex::String));
    
    // --- Numbers (después de strings) ---
    lang.mTokenRegexStrings.push_back(std::make_pair("\\b0[xX][0-9a-fA-F]+\\b", TextEditor::PaletteIndex::Number));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\b[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\b", TextEditor::PaletteIndex::Number));
    
    // --- Operators especiales (2+ caracteres) - ANTES de identifiers ---
    lang.mTokenRegexStrings.push_back(std::make_pair("=>", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\+\\+", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("--", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\*\\*", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("<=|>=|==|!=", TextEditor::PaletteIndex::Punctuation));
    
    // --- Comments (DESPUÉS de strings para evitar que # en strings se marque como comment) ---
    // El regex #[^\n]* coincide con # hasta fin de línea
    // Al procesarse después de los strings regex, los # dentro de strings no se marcan
    lang.mTokenRegexStrings.push_back(std::make_pair("#[^\\n]*", TextEditor::PaletteIndex::Comment));
    
    // --- Identifiers (ÚLTIMO en regex, después de keywords se verifican) ---
    lang.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
    
    // Opciones
    lang.mCaseSensitive = true;
    lang.mAutoIndentation = true;
    
    return lang;
}

inline TextEditor::Palette GetAvaLangPalette() {
    TextEditor::Palette pal = TextEditor::GetDarkPalette();
    
    // Keywords - NARANJA
    pal[(int)TextEditor::PaletteIndex::Keyword] = IM_COL32(255, 165, 0, 255);
    
    // Strings - AZUL CLARO
    pal[(int)TextEditor::PaletteIndex::String] = IM_COL32(86, 156, 214, 255);
    pal[(int)TextEditor::PaletteIndex::CharLiteral] = IM_COL32(86, 156, 214, 255);
    
    // Numbers - VERDE CLARO
    pal[(int)TextEditor::PaletteIndex::Number] = IM_COL32(181, 206, 168, 255);
    
    // Comments - VERDE
    pal[(int)TextEditor::PaletteIndex::Comment] = IM_COL32(106, 153, 78, 255);
    pal[(int)TextEditor::PaletteIndex::MultiLineComment] = IM_COL32(106, 153, 78, 255);
    
    // Known identifiers (para base, funciones built-in, etc.)
    pal[(int)TextEditor::PaletteIndex::KnownIdentifier] = IM_COL32(220, 220, 170, 255);
    
    return pal;
}

} // namespace studio