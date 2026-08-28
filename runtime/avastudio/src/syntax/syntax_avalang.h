#pragma once

#include "TextEditor.h"

namespace studio {

inline TextEditor::LanguageDefinition AvaLangLanguageDef() {
    TextEditor::LanguageDefinition lang;
    lang.mName = "AvaLang";

    lang.mSingleLineComment = "#";

    static const char* const keywords[] = {

        "if", "elif", "else", "while", "for", "then", "end", "in",

        "func", "class", "return", "base",

        "static", "private",

        "async", "await",

        "select", "case", "to", "is",

        "local",

        "break", "continue", "pass", "raise", "yield",

        "try", "catch", "finally",

        "import", "as", "extern",

        "true", "false", "nil",

        "and", "or", "not"
    };

    for (auto& kw : keywords) {
        lang.mKeywords.insert(kw);
    }

    lang.mTokenRegexStrings.push_back(std::make_pair("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", TextEditor::PaletteIndex::String));

    lang.mTokenRegexStrings.push_back(std::make_pair("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'", TextEditor::PaletteIndex::String));

    lang.mTokenRegexStrings.push_back(std::make_pair(R"(\$"[^"\$]*(?:\$[^"{]|\{[^}]*\})*[^\"]*")", TextEditor::PaletteIndex::String));

    lang.mTokenRegexStrings.push_back(std::make_pair("\\b0[xX][0-9a-fA-F]+\\b", TextEditor::PaletteIndex::Number));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\b[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\b", TextEditor::PaletteIndex::Number));

    lang.mTokenRegexStrings.push_back(std::make_pair("=>", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\+\\+", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("--", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("\\*\\*", TextEditor::PaletteIndex::Punctuation));
    lang.mTokenRegexStrings.push_back(std::make_pair("<=|>=|==|!=", TextEditor::PaletteIndex::Punctuation));

    lang.mTokenRegexStrings.push_back(std::make_pair("#[^\\n]*", TextEditor::PaletteIndex::Comment));

    lang.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));

    lang.mCaseSensitive = true;
    lang.mAutoIndentation = true;

    return lang;
}

inline TextEditor::Palette GetAvaLangPalette() {
    TextEditor::Palette pal = TextEditor::GetDarkPalette();

    pal[(int)TextEditor::PaletteIndex::Keyword] = IM_COL32(255, 165, 0, 255);

    pal[(int)TextEditor::PaletteIndex::String] = IM_COL32(86, 156, 214, 255);
    pal[(int)TextEditor::PaletteIndex::CharLiteral] = IM_COL32(86, 156, 214, 255);

    pal[(int)TextEditor::PaletteIndex::Number] = IM_COL32(181, 206, 168, 255);

    pal[(int)TextEditor::PaletteIndex::Comment] = IM_COL32(106, 153, 78, 255);
    pal[(int)TextEditor::PaletteIndex::MultiLineComment] = IM_COL32(106, 153, 78, 255);

    pal[(int)TextEditor::PaletteIndex::KnownIdentifier] = IM_COL32(220, 220, 170, 255);

    return pal;
}

}
