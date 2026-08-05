#ifndef AVA_UI_EVENTS_AUTO_BIND_H
#define AVA_UI_EVENTS_AUTO_BIND_H

#include <string>
#include <unordered_set>
#include <vector>

#include "Export.h"
#include "Fwd.h"

namespace avalang {
namespace ui {

AVA_UI_API const std::unordered_set<std::string>& EventPropNames();

AVA_UI_API bool IsEventPropertyName(const std::string& name);

AVA_UI_API void AutoBindEvents(IComponent* root, const std::string& codeText);

} // namespace ui
} // namespace avalang

#endif // AVA_UI_EVENTS_AUTO_BIND_H
