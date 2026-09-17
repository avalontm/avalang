#pragma once

#include "design/design_document.h"

namespace studio {

void DrawDocumentTreePanel(design::DesignDocument& doc, int tab_id, bool* p_open = nullptr);

void ReleaseDocumentTreeState(int tab_id);

}
