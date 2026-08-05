#ifndef AVA_UI_RESOLVER_KNOWN_COMPONENT_PROPERTIES_H
#define AVA_UI_RESOLVER_KNOWN_COMPONENT_PROPERTIES_H

#include <cstddef>

#include "Export.h"

namespace avalang {
namespace ui {

AVA_UI_API const char* const* KnownComponentPropertyNames(std::size_t& count);

} // namespace ui
} // namespace avalang

#endif // AVA_UI_RESOLVER_KNOWN_COMPONENT_PROPERTIES_H
