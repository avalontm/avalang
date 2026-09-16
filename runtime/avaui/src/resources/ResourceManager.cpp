#include "ResourceManager.h"
#include "ResourcePathResolver.h"
#include "layout/FontRegistry.h"

namespace avalang {
namespace ui {

ResourceManager::ResourceManager()
    : provider_(CreateDefaultResourceProvider()),
      localization_(CreateDefaultLocalizationProvider()) {}

ResourceManager& ResourceManager::Instance() {
    static ResourceManager instance;
    return instance;
}

std::string ResourceManager::ResolvePath(const std::string& logicalPath, ResourceType type) {
    if (!resources::HasLogicalPrefix(logicalPath)) {
        return logicalPath;
    }

    std::string resolved = provider_->ResolvePhysicalPath(logicalPath, type);
    if (resolved.empty()) {
        return logicalPath;
    }
    return resolved;
}

std::string ResourceManager::ResolveImagePath(const std::string& logicalPath) {
    return ResolvePath(logicalPath, ResourceType::Image);
}

std::string ResourceManager::ResolveIconPath(const std::string& logicalPath) {
    return ResolvePath(logicalPath, ResourceType::Icon);
}

bool ResourceManager::RegisterFont(const std::string& familyName, const std::string& filePath) {
    return layout::FontRegistry::Instance().RegisterFontFile(familyName, ResolvePath(filePath, ResourceType::Font));
}

std::string ResourceManager::Translate(const std::string& key, const std::string& fallback) {
    return localization_->Resolve(key, fallback);
}

bool ResourceManager::SetLanguage(const std::string& langCode) {
    return localization_->LoadLanguage(langCode);
}

std::string ResourceManager::CurrentLanguage() const {
    return localization_->CurrentLanguage();
}

void ResourceManager::ClearCache() {
    provider_->ClearCache();
}

}
}
