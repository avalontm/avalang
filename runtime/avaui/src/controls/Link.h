#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang::ui::controls {

AVA_UI_API IComponent* CreateLink(ComponentTree* tree, const std::string& text, const std::string& href);

AVA_UI_API void SetLinkText(IComponent* linkComponent, const std::string& text);

AVA_UI_API void SetLinkHref(IComponent* linkComponent, const std::string& href);

} // namespace avalang::ui::controls
