#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "render_tree/IRenderNode.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommand.h"

namespace avaui_tests {

inline std::string RenderNodeTypeName(avalang::ui::render::RenderNodeType type) {
    using avalang::ui::render::RenderNodeType;
    switch (type) {
        case RenderNodeType::Container: return "Container";
        case RenderNodeType::Row: return "Row";
        case RenderNodeType::Column: return "Column";
        case RenderNodeType::Stack: return "Stack";
        case RenderNodeType::Rectangle: return "Rectangle";
        case RenderNodeType::Text: return "Text";
        case RenderNodeType::Image: return "Image";
        case RenderNodeType::Path: return "Path";
        case RenderNodeType::Button: return "Button";
        case RenderNodeType::Checkbox: return "Checkbox";
        case RenderNodeType::Input: return "Input";
        case RenderNodeType::Label: return "Label";
        case RenderNodeType::Icon: return "Icon";
        case RenderNodeType::Custom: return "Custom";
        case RenderNodeType::Ellipse: return "Ellipse";
        case RenderNodeType::Slot: return "Slot";
        case RenderNodeType::ComboBox: return "ComboBox";
        case RenderNodeType::Link: return "Link";
        case RenderNodeType::Dialog: return "Dialog";
        case RenderNodeType::ScrollView: return "ScrollView";
        case RenderNodeType::RadioButton: return "RadioButton";
    }
    return "Unknown";
}

inline std::string RenderCommandTypeName(avalang::ui::RenderCommandType type) {
    using avalang::ui::RenderCommandType;
    switch (type) {
        case RenderCommandType::DrawRectangle: return "DrawRectangle";
        case RenderCommandType::DrawText: return "DrawText";
        case RenderCommandType::DrawImage: return "DrawImage";
        case RenderCommandType::PushClip: return "PushClip";
        case RenderCommandType::PopClip: return "PopClip";
        case RenderCommandType::Translate: return "Translate";
        case RenderCommandType::Scale: return "Scale";
        case RenderCommandType::Rotate: return "Rotate";
        case RenderCommandType::DrawEllipse: return "DrawEllipse";
        case RenderCommandType::DrawHtmlFragment: return "DrawHtmlFragment";
        case RenderCommandType::DrawButton: return "DrawButton";
        case RenderCommandType::DrawLink: return "DrawLink";
        case RenderCommandType::DrawPath: return "DrawPath";
        case RenderCommandType::DrawInput: return "DrawInput";
        case RenderCommandType::DrawCheckBox: return "DrawCheckBox";
        case RenderCommandType::DrawRadioButton: return "DrawRadioButton";
        case RenderCommandType::DrawComboBox: return "DrawComboBox";
    }
    return "Unknown";
}

inline std::string FormatBounds(double x, double y, double width, double height) {
    std::ostringstream out;
    out << static_cast<long>(x) << "," << static_cast<long>(y) << ","
        << static_cast<long>(width) << "," << static_cast<long>(height);
    return out.str();
}

inline void SnapshotRenderNode(const std::shared_ptr<avalang::ui::render::IRenderNode>& node,
                                int depth, std::ostringstream& out) {
    if (!node) return;

    const avalang::ui::LayoutRect rect = node->Rect();
    out << std::string(static_cast<size_t>(depth) * 2, ' ')
        << RenderNodeTypeName(node->Type())
        << " id=" << node->Id()
        << " bounds=" << FormatBounds(rect.x, rect.y, rect.width, rect.height)
        << "\n";

    for (const auto& child : node->Children()) {
        SnapshotRenderNode(child, depth + 1, out);
    }
}

inline std::string SnapshotRenderTree(const std::shared_ptr<avalang::ui::render::IRenderNode>& root) {
    std::ostringstream out;
    SnapshotRenderNode(root, 0, out);
    return out.str();
}

inline void SnapshotSceneNode(const std::shared_ptr<avalang::ui::scene::ISceneNode>& node,
                               int depth, std::ostringstream& out) {
    if (!node) return;

    const auto world = node->WorldTransform();
    out << std::string(static_cast<size_t>(depth) * 2, ' ')
        << RenderNodeTypeName(node->Type())
        << " id=" << node->Id()
        << " visible=" << (node->IsVisible() ? "true" : "false")
        << " opacity=" << node->Opacity()
        << " zOrder=" << node->ZOrder()
        << " x=" << static_cast<long>(world.position.x)
        << " y=" << static_cast<long>(world.position.y)
        << "\n";

    for (const auto& child : node->Children()) {
        SnapshotSceneNode(child, depth + 1, out);
    }
}

inline std::string SnapshotSceneGraph(avalang::ui::scene::ISceneGraph& scene) {
    std::ostringstream out;
    SnapshotSceneNode(scene.Root(), 0, out);
    return out.str();
}

inline std::string SnapshotRenderCommands(const std::vector<avalang::ui::RenderCommand>& commands) {
    std::ostringstream out;
    for (const auto& cmd : commands) {
        out << RenderCommandTypeName(cmd.type) << "\n";
    }
    return out.str();
}

}
