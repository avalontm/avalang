#pragma once

#include "Export.h"
#include "IResourceProvider.h"
#include "ILocalizationProvider.h"

#include <memory>
#include <string>

namespace avalang {
namespace ui {

class AVA_UI_API ResourceManager {
public:
    static ResourceManager& Instance();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    std::string ResolveImagePath(const std::string& logicalPath);
    std::string ResolveIconPath(const std::string& logicalPath);

    bool RegisterFont(const std::string& familyName, const std::string& filePath);

    std::string Translate(const std::string& key, const std::string& fallback = "");
    bool SetLanguage(const std::string& langCode);
    std::string CurrentLanguage() const;

    void ClearCache();

    IResourceProvider& Provider() { return *provider_; }
    ILocalizationProvider& Localization() { return *localization_; }

private:
    ResourceManager();

    std::string ResolvePath(const std::string& logicalPath, ResourceType type);

    std::unique_ptr<IResourceProvider> provider_;
    std::unique_ptr<ILocalizationProvider> localization_;
};

}
}
