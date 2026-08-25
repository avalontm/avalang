#pragma once

#include "plugin_api.h"

namespace studio {
namespace plugins_ui {

void FillUiApi(AvaUiApi& ui);

AvaPanelContext* BeginPanelContext(const char* panel_name);
void EndPanelContext(AvaPanelContext* ctx);

}
}
