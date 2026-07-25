#pragma once

#include <string>
#include <unordered_map>

#include "design/design_document.h"

namespace studio::design {

// Plain rectangle, deliberately NOT ImVec2/ImRect -- this module has no
// ImGui dependency (see 08_DESIGNER_VIEW_PLAN.md section 5.3: it's a
// pure data->rects calculation, the actual ImGui drawing/hit-testing
// lives in the future designer_canvas.cpp, which converts these into
// ImVec2s at the call site). x/y are relative to whatever origin the
// caller passed as `available_space` in ComputeLayout.
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

// Implements (a first slice of) the Column/Row/Stack layout described
// in docs/architecture/07_COMPONENT_TREE.md -- enough to turn a
// DesignNode tree into real rectangles the Designer canvas can draw
// and hit-test against. Grid/Flex aren't real layouts yet: any
// container type this doesn't specifically recognize ("grid", "flex",
// "page", or anything unknown) falls back to the same top-to-bottom
// stacking as "column", which is a reasonable default for a page root
// and gives Grid/Flex *a* rectangle instead of crashing/collapsing to
// zero size -- see 08_DESIGNER_VIEW_PLAN.md phase notes for when
// dedicated Grid/Flex algorithms are worth adding.
//
// A leaf node (no children) always gets a fixed default height
// (kDefaultLeafHeight below) -- there's no concept yet of a control's
// "natural" size (e.g. a Text node's height depending on font/wrap),
// that needs actual text measurement wired in once the canvas is
// drawing real ImGui widgets instead of wireframe boxes.
struct LayoutResult {
    // node_uid -> its computed rect, one entry per node in the tree
    // (root included). Unordered_map because the canvas only ever
    // looks up by uid (on click/hover), never iterates this in tree
    // order -- it walks the DesignNode tree itself for that and looks
    // up rects as it goes.
    std::unordered_map<std::string, Rect> rects;
};

// Default height (in the same units as `available_space`, i.e. ImGui
// pixels once the canvas passes real screen space in) given to any
// leaf node and to any container with no children. Exposed so the
// canvas/tests can reference the same constant instead of guessing it.
extern const float kDefaultLeafHeight;

LayoutResult ComputeLayout(const DesignNode& root, Rect available_space);

} // namespace studio::design
