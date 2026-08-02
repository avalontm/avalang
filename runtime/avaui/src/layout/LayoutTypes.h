#ifndef AVA_UI_LAYOUT_LAYOUTTYPES_H
#define AVA_UI_LAYOUT_LAYOUTTYPES_H

namespace avalang {
namespace ui {

// Axis-aligned box in layout space (logical units, DPI-independent --
// backends convert to pixels). Always represents a component's own
// border box: margin is space the *parent* consumes when placing the
// child, so it is never part of the child's own LayoutRect. The
// Layout Engine never draws -- this is pure geometry, consumed later
// by the Render Tree (Phase 6+).
struct LayoutRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

// Alignment of a component within the space its parent offers it,
// independently per axis (see LayoutEngine / LayoutProperties.h for
// how "align", "align-h" and "align-v" map to this). "Stretch" (the
// default) fills the offered space; the other three keep the
// component's own size (explicit `width`/`height` property) and
// position it inside the offered space.
enum class LayoutAlignment {
    Start,
    Center,
    End,
    Stretch,
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_LAYOUTTYPES_H
