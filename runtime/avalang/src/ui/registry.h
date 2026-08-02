#ifndef AVA_UI_REGISTRY_H
#define AVA_UI_REGISTRY_H

#include <string>
#include <vector>

namespace ava {
namespace ui {

// Registry of known component type names — matches the Toolbox list from
// the Ava Studio vision doc (Containers: Stack/Grid/Panel/Canvas;
// Controls: Button/Label/TextBox/Image/CheckBox/RadioButton; etc).
//
// NOTE: unlike component.h/tree.h, no call site for this file was found
// anywhere else in this snapshot (compiler/vm/c_api never reference it).
// Reconstructed as a minimal, self-consistent placeholder so the CMake
// source list resolves; extend or replace once you confirm what this
// file actually did.
class ComponentRegistry {
public:
    static bool IsKnownType(const std::string& type);
    static const std::vector<std::string>& KnownTypes();
};

} // namespace ui
} // namespace ava

#endif // AVA_UI_REGISTRY_H
