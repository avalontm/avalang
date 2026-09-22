#include "languages/avaui_language.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "languages/avalang_language.h"
#include "registry/ComponentTypeRegistry.h"

namespace studio::languages {

namespace {

TextEditor::Iterator ReadIdentifier(TextEditor::Iterator start, TextEditor::Iterator end) {
    TextEditor::Iterator it = start;
    if (it < end && TextEditor::CodePoint::isXidStart(*it)) {
        ++it;
        while (it < end && TextEditor::CodePoint::isXidContinue(*it)) ++it;
    }
    return it;
}

const std::unordered_set<std::string>& AvauiComponentTypeNames() {
    static const std::unordered_set<std::string> names = [] {
        std::unordered_set<std::string> result;
        for (const auto& descriptor : avalang::ui::registry::GetComponentTypeRegistry()) {
            result.insert(descriptor.type);
        }
        return result;
    }();
    return names;
}

template <typename Base>
class AvauiTokenizer {
public:
    explicit AvauiTokenizer(Base base) : base_(std::move(base)) {}

    TextEditor::Iterator operator()(TextEditor::Iterator start, TextEditor::Iterator end,
                                     TextEditor::Color& color) {
        if (!has_last_line_ || !(end == last_line_end_)) {
            at_line_start_ = true;
            has_last_line_ = true;
            last_line_end_ = end;
        }

        const bool was_at_line_start = at_line_start_;
        at_line_start_ = false;

        if (was_at_line_start) {
            TextEditor::Iterator word_end = ReadIdentifier(start, end);
            if (word_end != start) {
                std::string word;
                for (TextEditor::Iterator it = start; it != word_end; ++it) {
                    word.push_back(static_cast<char>(*it));
                }
                if (AvauiComponentTypeNames().count(word)) {
                    color = TextEditor::Color::preprocessor;
                    return word_end;
                }
            }
        }

        return base_(start, end, color);
    }

private:
    Base base_;
    TextEditor::Iterator last_line_end_;
    bool has_last_line_ = false;
    bool at_line_start_ = true;
};

}

const TextEditor::Language* AvauiLang() {
    static const TextEditor::Language language = [] {
        TextEditor::Language lang = *AvaLang();
        lang.name = "Avaui";

        lang.keywords.insert("view");
        lang.keywords.insert("param");
        lang.keywords.insert("params");
        lang.keywords.insert("state");
        lang.keywords.insert("const");
        lang.keywords.insert("style");
        lang.keywords.insert("properties");
        lang.keywords.insert("metadata");
        lang.keywords.insert("extends");
        lang.keywords.insert("route");
        lang.keywords.insert("code");
        lang.keywords.insert("methods");

        lang.customTokenizer = AvauiTokenizer<decltype(lang.customTokenizer)>(lang.customTokenizer);

        return lang;
    }();
    return &language;
}

}
