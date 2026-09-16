#include "layout/TextMeasure.h"

#include "layout/FontRegistry.h"

namespace avalang {
namespace ui {
namespace layout {

namespace {

constexpr double kWrapLineSpacingMultiplier = 1.25;

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

}

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

}
}
}