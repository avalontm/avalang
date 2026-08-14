#include "layout/TextMeasure.h"

#include "layout/FontRegistry.h"

#include <sstream>

namespace avalang {
namespace ui {
namespace layout {

namespace {

// Extra leading applied on top of the font's raw line height when
// stacking wrapped lines -- matches a browser's default (unset)
// `line-height`, which is typically ~1.15-1.3x the font's own
// ascent+descent rather than exactly 1.0x. Picked once here so
// LayoutEngineImpl's intrinsic-height math and every renderer's
// per-line y-offset always agree.
constexpr double kWrapLineSpacingMultiplier = 1.25;

// Splits `text` on ASCII whitespace into words, preserving explicit
// '\n' as its own token (so WrapTextLines can treat it as a forced
// break) -- everything else (spaces/tabs) is just a separator and is
// dropped, same as HTML's own whitespace collapsing would do to text
// that reaches OnDrawText.
std::vector<std::string> Tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string word;
    auto flush = [&]() {
        if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    };
    for (char c : text) {
        if (c == '\n') {
            flush();
            tokens.push_back("\n");
        } else if (c == ' ' || c == '\t' || c == '\r') {
            flush();
        } else {
            word += c;
        }
    }
    flush();
    return tokens;
}

} // namespace

double EstimateTextWidth(const std::string& text, double fontSize, const std::string& fontName) {
    if (text.empty() || fontSize <= 0.0) {
        return 0.0;
    }
    return FontRegistry::Instance().MeasureTextWidth(text, fontSize, fontName);
}

double DefaultLineHeight(double fontSize, const std::string& fontName) {
    if (fontSize <= 0.0) {
        return 0.0;
    }
    return FontRegistry::Instance().LineHeight(fontSize, fontName);
}

double WrappedLineHeight(double fontSize, const std::string& fontName) {
    return DefaultLineHeight(fontSize, fontName) * kWrapLineSpacingMultiplier;
}

std::vector<std::string> WrapTextLines(const std::string& text, double fontSize,
                                        const std::string& fontName, double maxWidth) {
    if (text.empty()) {
        return {std::string()};
    }
    if (maxWidth <= 0.0 || fontSize <= 0.0) {
        return {text};
    }
    // Fast path: the whole string already fits on one line -- avoids
    // tokenizing/re-measuring every draw for the overwhelming majority
    // of Text/Label/Button/Link content, which never opts into `wrap`
    // in the first place.
    if (EstimateTextWidth(text, fontSize, fontName) <= maxWidth) {
        return {text};
    }

    const double spaceWidth = EstimateTextWidth(" ", fontSize, fontName);
    std::vector<std::string> tokens = Tokenize(text);
    std::vector<std::string> lines;
    std::string currentLine;
    double currentWidth = 0.0;

    auto pushLine = [&]() {
        lines.push_back(currentLine);
        currentLine.clear();
        currentWidth = 0.0;
    };

    for (const std::string& token : tokens) {
        if (token == "\n") {
            pushLine();
            continue;
        }

        const double wordWidth = EstimateTextWidth(token, fontSize, fontName);

        if (currentLine.empty()) {
            // First word on a line always goes on it, even if the
            // word alone is wider than maxWidth -- never split a word
            // mid-character; the line will just overflow slightly
            // rather than lose text.
            currentLine = token;
            currentWidth = wordWidth;
            continue;
        }

        const double candidateWidth = currentWidth + spaceWidth + wordWidth;
        if (candidateWidth <= maxWidth) {
            currentLine += " ";
            currentLine += token;
            currentWidth = candidateWidth;
        } else {
            pushLine();
            currentLine = token;
            currentWidth = wordWidth;
        }
    }
    if (!currentLine.empty() || lines.empty()) {
        pushLine();
    }

    return lines;
}

} // namespace layout
} // namespace ui
} // namespace avalang
