#pragma once
#include <cstddef>

namespace avahost {

// Canonical list of `.avaui` property names that AvaHost's rendering
// pipeline reads by literal name outside of IComponent itself (IComponent
// has no PropertyNames enumerator -- the interface was frozen at Fase 13,
// see ui_component_resolver.cpp). Two call sites need this exact list and
// used to keep independent copies that drifted apart:
//
//   - ui_component_resolver.cpp (UiComponentResolver::CloneInto) copies
//     these properties when it inlines an imported component's tree
//     (`import components.Navbar` + `Navbar()`) into the page that called
//     it. Any property missing here silently disappears from every
//     imported component instance -- it is never a parse error, the
//     element just renders as if the property had never been set.
//   - ui_vm_event_bridge.cpp (BindComponentRefsNative /
//     ExportComponentPropsNative) uses the same list to expose/collect
//     `id.property` globals to AvaLang code between VM calls.
//
// Before this list was unified, "textColor" (RenderTree.cpp, foreground
// color for Text/Label/Button), "click" (RenderTree.cpp, click handler
// name -> RenderNode::SetClickHandler) and "selectedValue" (ComboBox.cpp
// / RenderTree.cpp, selected option) were read by both of those call
// sites but were absent from the allowlist: any Button/Text/ComboBox
// declared *inside* an imported component (rather than directly in the
// page) lost its text color, its click handler, and its selected value
// the moment the import resolver cloned it -- the same "property doesn't
// survive the pipeline" bug already found for `class` and `textColor` at
// the RenderTree layer.
inline const char* const* KnownComponentPropertyNames(std::size_t& count) {
    static const char* kNames[] = {
        "id", "text", "value", "placeholder", "class", "style",
        "backgroundColor", "borderColor", "color", "textColor",
        "fontSize", "fontName",
        "source", "alt", "label", "checked", "isChecked", "isSelected",
        "selectedValue",
        "group", "href", "data", "align", "justify", "padding",
        "margin", "gap", "width", "height", "radius", "borderRadius",
        "borderWidth", "background", "fill", "spacing",
        "click", "onmouseenter", "onmouseleave", "onfocus", "onblur",
        "onkeydown", "onkeyup"
    };
    count = sizeof(kNames) / sizeof(kNames[0]);
    return kNames;
}

} // namespace avahost
