#ifndef AVA_UI_RESOLVER_DOTTED_PATH_H
#define AVA_UI_RESOLVER_DOTTED_PATH_H

#include <filesystem>
#include <string>

#include "Export.h"

namespace avalang {
namespace ui {

AVA_UI_API std::filesystem::path ResolveDottedAvauiPath(const std::string& projectRoot,
                                                        const std::string& dotted);

AVA_UI_API std::string CallableTagFromDotted(const std::string& dotted);

} // namespace ui
} // namespace avalang

#endif // AVA_UI_RESOLVER_DOTTED_PATH_H
