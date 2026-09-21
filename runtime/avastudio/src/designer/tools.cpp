#include "designer/tools.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "designer/command.h"
#include "designer/component_registry.h"
#include "designer/document_commands.h"
#include "designer/selection_manager.h"

namespace studio::designer {

namespace {

std::string ToLowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}

void SelectTool::Select(SelectionManager* selection, const NodeId& nodeId) {
    if (selection) selection->Select(nodeId);
}

void SelectTool::Clear(SelectionManager* selection) {
    if (selection) selection->Clear();
}

bool MoveTool::CanMove(const UiNode* node, const UiNode* root) {
    return node != nullptr && node != root;
}

bool MoveTool::Execute(CommandManager* manager, design::DesignDocument& document, SelectionManager* selection,
                        const NodeId& movedId, const NodeId& targetId, design::DropZone zone) {
    return ExecuteMoveComponent(manager, document, selection, movedId, targetId, zone);
}

bool ResizeTool::CanResize(const UiNode* node, const UiNode* root) {
    if (node == nullptr || node == root) return false;
    const ComponentMetadata* metadata = ComponentRegistry::Instance().Find(ToLowerAscii(node->TypeName()));
    return metadata == nullptr || metadata->designerCapabilities.canResize;
}

void ResizeTool::Begin(const NodeId& nodeId, bool resizeX, bool resizeY, float startWidth, float startHeight) {
    active_ = true;
    nodeId_ = nodeId;
    resizeX_ = resizeX;
    resizeY_ = resizeY;
    startWidth_ = startWidth;
    startHeight_ = startHeight;
    previewWidth_ = startWidth;
    previewHeight_ = startHeight;
}

void ResizeTool::UpdateDelta(float deltaX, float deltaY, float minDimension) {
    if (!active_) return;
    if (resizeX_) previewWidth_ = std::max(minDimension, startWidth_ + deltaX);
    if (resizeY_) previewHeight_ = std::max(minDimension, startHeight_ + deltaY);
}

void ResizeTool::ApplySnap(double dx, double dy) {
    if (!active_) return;
    if (resizeX_) previewWidth_ = std::max(0.0f, previewWidth_ + static_cast<float>(dx));
    if (resizeY_) previewHeight_ = std::max(0.0f, previewHeight_ + static_cast<float>(dy));
}

bool ResizeTool::Commit(CommandManager* manager, design::DesignDocument& document, SelectionManager* selection) {
    if (!active_) return false;
    bool changed = false;
    if (resizeX_) {
        changed = ExecuteSetProperty(manager, document, selection, nodeId_, "width",
                                      std::to_string(static_cast<int>(std::lround(previewWidth_)))) ||
                  changed;
    }
    if (resizeY_) {
        changed = ExecuteSetProperty(manager, document, selection, nodeId_, "height",
                                      std::to_string(static_cast<int>(std::lround(previewHeight_)))) ||
                  changed;
    }
    active_ = false;
    return changed;
}

void ResizeTool::Cancel() {
    active_ = false;
}

std::string InsertTool::InsertInto(CommandManager* manager, design::DesignDocument& document,
                                    SelectionManager* selection, const NodeId& containerId,
                                    const std::string& type) {
    return ExecuteAddComponent(manager, document, selection, containerId, type, "", {});
}

std::string InsertTool::InsertRelative(CommandManager* manager, design::DesignDocument& document,
                                        SelectionManager* selection, const NodeId& siblingParentId,
                                        const NodeId& siblingId, design::DropZone zone, const std::string& type) {
    const bool ownsTransaction = manager != nullptr && !manager->InTransaction();
    if (ownsTransaction) manager->BeginTransaction("Insert " + type);

    const std::string newId = ExecuteAddComponent(manager, document, selection, siblingParentId, type, "", {});
    if (!newId.empty()) {
        ExecuteMoveComponent(manager, document, selection, newId, siblingId, zone);
    }

    if (ownsTransaction) manager->EndTransaction();
    return newId;
}

void PanTool::Pan(DesignerViewport* viewport, double dx, double dy) {
    if (viewport) viewport->Pan(dx, dy);
}

void PanTool::SetPan(DesignerViewport* viewport, double x, double y) {
    if (viewport) viewport->SetPan(x, y);
}

void PanTool::Reset(DesignerViewport* viewport) {
    if (viewport) viewport->Reset();
}

void ZoomTool::ZoomBy(DesignerViewport* viewport, double factor, const LayoutPoint& anchor) {
    if (!viewport || factor <= 0.0) return;
    viewport->SetZoom(viewport->Zoom() * factor, anchor);
}

void ZoomTool::ZoomIn(DesignerViewport* viewport, const LayoutPoint& anchor) {
    ZoomBy(viewport, 1.1, anchor);
}

void ZoomTool::ZoomOut(DesignerViewport* viewport, const LayoutPoint& anchor) {
    ZoomBy(viewport, 1.0 / 1.1, anchor);
}

void ZoomTool::ZoomTo(DesignerViewport* viewport, double zoom, const LayoutPoint& anchor) {
    if (viewport) viewport->SetZoom(zoom, anchor);
}

void ZoomTool::FitToScreen(DesignerViewport* viewport, const LayoutSize& contentSize, const LayoutSize& screenSize) {
    if (viewport) viewport->FitToScreen(contentSize, screenSize);
}

void ZoomTool::ResetZoom(DesignerViewport* viewport) {
    if (viewport) viewport->Reset();
}

}
