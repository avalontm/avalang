#pragma once

#include <string>

#include "design/design_document.h"
#include "designer/types.h"
#include "designer/viewport.h"

namespace studio::designer {

class CommandManager;
class SelectionManager;

class SelectTool {
public:
    static void Select(SelectionManager* selection, const NodeId& nodeId);
    static void Clear(SelectionManager* selection);
};

class MoveTool {
public:
    static bool CanMove(const UiNode* node, const UiNode* root);

    static bool Execute(CommandManager* manager, design::DesignDocument& document, SelectionManager* selection,
                         const NodeId& movedId, const NodeId& targetId, design::DropZone zone);
};

class ResizeTool {
public:
    static bool CanResize(const UiNode* node, const UiNode* root);

    void Begin(const NodeId& nodeId, bool resizeX, bool resizeY, float startWidth, float startHeight);
    void UpdateDelta(float deltaX, float deltaY, float minDimension);
    void ApplySnap(double dx, double dy);
    bool Commit(CommandManager* manager, design::DesignDocument& document, SelectionManager* selection);
    void Cancel();

    bool IsActive() const { return active_; }
    bool IsDragging(const NodeId& nodeId) const { return active_ && nodeId_ == nodeId; }
    bool ResizesX() const { return resizeX_; }
    bool ResizesY() const { return resizeY_; }
    float PreviewWidth() const { return previewWidth_; }
    float PreviewHeight() const { return previewHeight_; }

private:
    bool active_ = false;
    NodeId nodeId_;
    bool resizeX_ = false;
    bool resizeY_ = false;
    float startWidth_ = 0.0f;
    float startHeight_ = 0.0f;
    float previewWidth_ = 0.0f;
    float previewHeight_ = 0.0f;
};

class InsertTool {
public:
    static std::string InsertInto(CommandManager* manager, design::DesignDocument& document,
                                   SelectionManager* selection, const NodeId& containerId, const std::string& type);

    static std::string InsertRelative(CommandManager* manager, design::DesignDocument& document,
                                       SelectionManager* selection, const NodeId& siblingParentId,
                                       const NodeId& siblingId, design::DropZone zone, const std::string& type);
};

class PanTool {
public:
    static void Pan(DesignerViewport* viewport, double dx, double dy);
    static void SetPan(DesignerViewport* viewport, double x, double y);
    static void Reset(DesignerViewport* viewport);
};

class ZoomTool {
public:
    static void ZoomBy(DesignerViewport* viewport, double factor, const LayoutPoint& anchor);
    static void ZoomIn(DesignerViewport* viewport, const LayoutPoint& anchor);
    static void ZoomOut(DesignerViewport* viewport, const LayoutPoint& anchor);
    static void ZoomTo(DesignerViewport* viewport, double zoom, const LayoutPoint& anchor);
    static void FitToScreen(DesignerViewport* viewport, const LayoutSize& contentSize, const LayoutSize& screenSize);
    static void ResetZoom(DesignerViewport* viewport);
};

}
