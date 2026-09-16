#pragma once

#include "Export.h"
#include <string>

namespace avalang {
namespace ui {
namespace resources {

enum class ResourceBackend {
    Web,
    Desktop,
};

AVA_UI_API bool HasLogicalPrefix(const std::string& path);

AVA_UI_API std::string ResolveResourcePath(const std::string& logicalPath, ResourceBackend backend);

}
}
}