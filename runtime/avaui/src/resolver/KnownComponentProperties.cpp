#include "resolver/KnownComponentProperties.h"

namespace avalang {
namespace ui {

const char* const* KnownComponentPropertyNames(std::size_t& count) {
    static const char* kNames[] = {
        "id", "text", "value", "placeholder", "class", "style",
        "backgroundColor", "borderColor", "textColor",
        "fontSize", "fontName",
        "source", "alt", "label", "checked", "isChecked", "isSelected",
        "selectedValue",
        "group", "href", "data", "align", "justify", "padding",
        "margin", "gap", "width", "height", "radius", "borderRadius",
        "borderWidth", "background", "fill", "spacing", "grow",
        "direction", "as",
        "click",
        "onfocus", "onblur", "onkeydown", "onkeyup"
    };
    count = sizeof(kNames) / sizeof(kNames[0]);
    return kNames;
}

}
}
