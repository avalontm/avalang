#include "designer/code_sync.h"

#include <algorithm>
#include <unordered_map>

#include "designer/property_editor.h"
#include "parser/AvauiWriter.h"

namespace studio::designer {

namespace {

std::unordered_map<std::string, std::string> PropertyMap(const UiNode* node) {
    std::unordered_map<std::string, std::string> map;
    for (const std::string& name : node->PropertyNames()) {
        if (const PropertyValue* value = node->GetProperty(name)) {
            map[name] = FormatPropertyValue(*value);
        }
    }
    return map;
}

}

DocumentState Transition(DocumentState current, DocumentEvent event) {
    switch (event) {
        case DocumentEvent::Edit:
            return DocumentState::Dirty;
        case DocumentEvent::SaveStarted:
            return DocumentState::Saving;
        case DocumentEvent::SaveSucceeded:
            return DocumentState::Saved;
        case DocumentEvent::SaveFailed:
            return DocumentState::Error;
        case DocumentEvent::Reload:
            return DocumentState::Clean;
    }
    return current;
}

bool NodesEquivalent(const UiNode* a, const UiNode* b, std::vector<std::string>* differences) {
    if (!a || !b) {
        const bool same = a == b;
        if (!same && differences) {
            differences->push_back("node presence mismatch");
        }
        return same;
    }

    bool equivalent = true;

    if (TypeOf(a) != TypeOf(b)) {
        equivalent = false;
        if (differences) {
            differences->push_back("type: " + TypeOf(a) + " != " + TypeOf(b));
        } else {
            return false;
        }
    }

    const std::unordered_map<std::string, std::string> propsA = PropertyMap(a);
    const std::unordered_map<std::string, std::string> propsB = PropertyMap(b);
    if (propsA != propsB) {
        equivalent = false;
        if (!differences) {
            return false;
        }
        for (const auto& [name, value] : propsA) {
            auto it = propsB.find(name);
            if (it == propsB.end()) {
                differences->push_back("missing property in generated: " + name);
            } else if (it->second != value) {
                differences->push_back("property '" + name + "' differs: '" + value + "' != '" + it->second + "'");
            }
        }
        for (const auto& [name, value] : propsB) {
            if (!propsA.count(name)) {
                differences->push_back("unexpected property in generated: " + name);
            }
        }
    }

    const std::vector<UiNode*> childrenA = a->Children();
    const std::vector<UiNode*> childrenB = b->Children();
    if (childrenA.size() != childrenB.size()) {
        equivalent = false;
        if (differences) {
            differences->push_back("child count differs: " + std::to_string(childrenA.size()) + " != " +
                                    std::to_string(childrenB.size()));
        } else {
            return false;
        }
    }

    const size_t count = std::min(childrenA.size(), childrenB.size());
    for (size_t i = 0; i < count; ++i) {
        if (!NodesEquivalent(childrenA[i], childrenB[i], differences)) {
            equivalent = false;
            if (!differences) {
                return false;
            }
        }
    }

    return equivalent;
}

RoundTripResult VerifyRoundTrip(const std::string& sourceText, const std::string& sourcePath) {
    RoundTripResult result;

    studio::design::DesignDocument original;
    if (!studio::design::ParseAvauiText(sourceText, original, result.error, sourcePath)) {
        return result;
    }
    result.parsedOriginal = true;

    result.generatedText =
        avalang::ui::parser::WriteAvaui(original.Root(), studio::design::BuildWriteOptions(original));

    studio::design::DesignDocument regenerated;
    if (!studio::design::ParseAvauiText(result.generatedText, regenerated, result.error, sourcePath)) {
        return result;
    }
    result.parsedGenerated = true;

    result.equivalent = NodesEquivalent(original.Root(), regenerated.Root(), &result.differences);
    return result;
}

}