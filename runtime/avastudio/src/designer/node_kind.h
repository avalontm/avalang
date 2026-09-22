#pragma once

#include <string>

#include "designer/types.h"

namespace studio::designer {

std::string LowerAscii(const std::string& text);

bool IsContainerType(const std::string& typeName);

bool IsDialogNode(const UiNode* node);

}
