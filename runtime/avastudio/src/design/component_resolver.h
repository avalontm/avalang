#pragma once

#include <memory>
#include <string>
#include <vector>

#include "components/ComponentTree.h"
#include "design/design_document.h"

namespace studio::design {

void ResolveImportsForDocument(DesignDocument& doc, const std::string& projectRoot);

std::unique_ptr<avalang::ui::ComponentTree> ResolvePreviewTree(const DesignDocument& doc,
                                                                 const std::string& projectRoot);

}
