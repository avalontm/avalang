#pragma once

#include <string>
#include <vector>

#include "design/design_document.h"

namespace studio::design {

void ResolveImportsForDocument(DesignDocument& doc, const std::string& projectRoot);

} // namespace studio::design
