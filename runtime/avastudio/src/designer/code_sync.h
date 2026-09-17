#pragma once

#include <string>
#include <vector>

#include "design/design_document.h"
#include "designer/types.h"

namespace studio::designer {

enum class DesignerSyncMode {
    Design,
    Code,
    Split,
};

enum class DocumentState {
    Clean,
    Dirty,
    Saving,
    Saved,
    Error,
};

enum class DocumentEvent {
    Edit,
    SaveStarted,
    SaveSucceeded,
    SaveFailed,
    Reload,
};

DocumentState Transition(DocumentState current, DocumentEvent event);

bool NodesEquivalent(const UiNode* a, const UiNode* b, std::vector<std::string>* differences = nullptr);

struct RoundTripResult {
    bool parsedOriginal = false;
    bool parsedGenerated = false;
    bool equivalent = false;
    std::string generatedText;
    std::string error;
    std::vector<std::string> differences;
};

RoundTripResult VerifyRoundTrip(const std::string& sourceText, const std::string& sourcePath = "");

}
