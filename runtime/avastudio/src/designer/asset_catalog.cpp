#include "designer/asset_catalog.h"

#include <algorithm>
#include <cctype>

namespace studio::designer {

namespace {

std::string LowerExtension(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}

AssetKind ClassifyAssetPath(const std::string& path) {
    const std::string ext = LowerExtension(path);

    if (ext == ".ttf" || ext == ".otf" || ext == ".fon") {
        return AssetKind::Font;
    }
    if (ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif") {
        return AssetKind::Image;
    }
    if (ext == ".lang" || ext == ".txt") {
        return AssetKind::Localization;
    }

    return AssetKind::Unknown;
}

bool IsRelativeAssetPath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (path.front() == '/' || path.front() == '\\') {
        return false;
    }
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        return false;
    }
    if (path.find("://") != std::string::npos) {
        return false;
    }

    return true;
}

}
