#pragma once

#include <string>

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "layout/LayoutProperties.h"
#include "layout/LayoutTypes.h"
#include "render_tree/IRenderNode.h"
#include "render_tree/IRenderTree.h"
#include "renderer/IRenderer.h"

namespace studio::designer {

using NodeId = std::string;
using ComponentTypeId = std::string;

using UiNode = avalang::ui::IComponent;
using UiComponentTree = avalang::ui::ComponentTree;
using UiComponentId = avalang::ui::ComponentId;

using RenderNode = avalang::ui::render::IRenderNode;
using RenderNodeType = avalang::ui::render::RenderNodeType;
using RenderTree = avalang::ui::render::IRenderTree;
using Renderer = avalang::ui::IRenderer;

using LayoutRect = avalang::ui::LayoutRect;
using LayoutPoint = avalang::ui::LayoutPoint;
using LayoutSize = avalang::ui::LayoutSize;
using LayoutConstraints = avalang::ui::LayoutConstraints;
using LayoutAlignment = avalang::ui::LayoutAlignment;
using EdgeInsets = avalang::ui::layout::EdgeInsets;

using PropertyType = avalang::ui::PropertyType;
using PropertyValue = avalang::ui::PropertyValue;
using PropertyRecord = avalang::ui::PropertyRecord;
using PropertyList = avalang::ui::PropertyList;

inline NodeId IdOf(const UiNode* node) {
    return node ? node->NodeId() : NodeId();
}

inline ComponentTypeId TypeOf(const UiNode* node) {
    return node ? node->TypeName() : ComponentTypeId();
}

}
