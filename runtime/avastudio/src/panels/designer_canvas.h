#pragma once

#include <optional>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "designer/overlay.h"
#include "imgui.h"
#include "panels/properties_panel.h"
#include "util/log_bridge.h"

struct AvaVM;

namespace studio::designer {
class CommandManager;
class SelectionManager;
class LayoutCore;
class DesignerViewport;
enum class CanvasMode;
}

namespace avalang::ui::animation {
class AnimationController;
}

namespace studio {

constexpr const char* kNodeMoveDragDropId = "AVAUI_NODE_MOVE";

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root = "",
                                                   int tab_id = -1,
                                                   std::string* out_generated_handler = nullptr,
                                                   LogBridge* log_bridge = nullptr);

void InvalidateDesignerVmCache(int tab_id);

void ReleaseDesignerTabState(int tab_id);

void ClearDesignerCommandHistory(int tab_id);

void RevertDesignerInjectedProperties(int tab_id, design::DesignDocument& doc);

void ClearDesignerSelection(int tab_id);

AvaVM* GetDesignerStateVM(int tab_id);

designer::CommandManager* GetDesignerCommandManager(int tab_id);

designer::SelectionManager* GetDesignerSelectionManager(int tab_id);

designer::LayoutCore* GetDesignerSurfaceLayout(int tab_id);

designer::DesignerViewport* GetDesignerViewport(int tab_id);

std::string GetDesignerHoveredNode(int tab_id);

designer::CanvasMode GetDesignerCanvasMode(int tab_id);

avalang::ui::animation::AnimationController* GetDesignerAnimationController(int tab_id);

std::vector<designer::OverlayItem> GetDesignerOverlay(int tab_id);

}
