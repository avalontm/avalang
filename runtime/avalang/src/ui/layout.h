#ifndef AVA_UI_LAYOUT_H
#define AVA_UI_LAYOUT_H

#include <string>

#include "ui/component.h"

namespace ava {
namespace ui {

// Human-readable name for a LayoutType, e.g. for JSON export
// (see ComponentToJson in public/src/c_api.cpp) or designer UI.
const char* LayoutTypeName(LayoutType layout);

// Parses a layout name (case-sensitive, matches LayoutTypeName output)
// back into a LayoutType. Returns LayoutType::None if unrecognized.
LayoutType LayoutTypeFromName(const std::string& name);

} // namespace ui
} // namespace ava

#endif // AVA_UI_LAYOUT_H
