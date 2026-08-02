#include "ui/layout.h"

namespace ava {
namespace ui {

const char* LayoutTypeName(LayoutType layout) {
    switch (layout) {
        case LayoutType::Column: return "Column";
        case LayoutType::Row:    return "Row";
        case LayoutType::Stack:  return "Stack";
        case LayoutType::Grid:   return "Grid";
        case LayoutType::Flex:   return "Flex";
        case LayoutType::None:
        default:
            return "None";
    }
}

LayoutType LayoutTypeFromName(const std::string& name) {
    if (name == "Column") return LayoutType::Column;
    if (name == "Row")    return LayoutType::Row;
    if (name == "Stack")  return LayoutType::Stack;
    if (name == "Grid")   return LayoutType::Grid;
    if (name == "Flex")   return LayoutType::Flex;
    return LayoutType::None;
}

} // namespace ui
} // namespace ava
