#pragma once

#include <string>

#include "designer/types.h"

namespace studio::designer {

struct ExtractionCandidate {
    bool valid = false;
    std::string reason;
};

ExtractionCandidate ValidateExtractionCandidate(UiComponentTree* tree, NodeId nodeId);

}
