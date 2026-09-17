#pragma once

#include <string>

#include "resources/IResourceProvider.h"

namespace studio::designer {

using AssetKind = avalang::ui::ResourceType;

AssetKind ClassifyAssetPath(const std::string& path);

bool IsRelativeAssetPath(const std::string& path);

}
