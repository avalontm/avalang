#include "resources/fonts/DefaultFontData.h"

#include "resources/fonts/default_font_regular_ttf.h"
#include "resources/fonts/default_font_bold_ttf.h"

namespace avalang {
namespace ui {
namespace fonts {

FontBytes DefaultRegular() {
    return FontBytes{kDefaultFontRegularTTF, static_cast<std::size_t>(kDefaultFontRegularTTFLen)};
}

FontBytes DefaultBold() {
    return FontBytes{kDefaultFontBoldTTF, static_cast<std::size_t>(kDefaultFontBoldTTFLen)};
}

}
}
}