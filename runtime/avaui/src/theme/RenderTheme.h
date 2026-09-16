#pragma once

#include "components/ComponentTree.h"
#include "ITheme.h"
#include "theme/ProjectStyleOverrides.h"
#include "Export.h"
#include <memory>

namespace avalang {
namespace ui {

class AVA_UI_API RenderTheme {
public:
    static bool Apply(ComponentTree* tree, ITheme* theme,
                       const theme::ProjectStyleSheet* styles = nullptr);

    static bool ApplyToComponent(IComponent* component, ITheme* theme,
                                  const theme::ProjectStyleSheet* styles = nullptr,
                                  bool isRoot = false);
};

}
}