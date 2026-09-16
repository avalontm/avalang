#pragma once

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

}
}