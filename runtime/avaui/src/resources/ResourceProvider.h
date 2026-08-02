#pragma once

#include "IResourceProvider.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace avalang::ui {

/**
 * Windows filesystem-based resource provider.
 * 
 * - Resolves logical paths (@fonts/Arial) to physical filesystem paths
 * - Caches loaded resources in memory (LRU or simple map)
 * - Supports multiple file extensions per type (e.g., .ttf, .otf for fonts)
 * - Case-insensitive path lookup
 */
class ResourceProvider : public IResourceProvider {
public:
    ResourceProvider();
    virtual ~ResourceProvider();

    Resource Load(const std::string& logicalPath, ResourceType type) override;
    bool RegisterPrefix(const std::string& prefix, const std::string& physicalPath) override;
    bool Exists(const std::string& logicalPath) override;
    void ClearCache() override;
    uint32_t AbiVersion() const override { return 15; }

private:
    struct CachedResource {
        std::vector<uint8_t> buffer;
        ResourceMetadata metadata;
    };

    struct PrefixEntry {
        std::string physicalPath;
    };

    std::unordered_map<std::string, CachedResource> cache_;
    std::unordered_map<std::string, PrefixEntry> prefixes_;

    // Helper to normalize paths (lowercase, forward slashes)
    static std::string Normalize(const std::string& path);

    // Helper to try loading a file with multiple extensions
    std::unique_ptr<std::vector<uint8_t>> TryLoadWithExtensions(
        const std::string& baseDir,
        const std::string& baseName,
        ResourceType type
    );

    // Platform-specific initialization
    void InitializeDefaultPrefixes();

    // Load image metadata (width/height/stride)
    bool LoadImageMetadata(const std::string& filePath, ResourceMetadata& out);
};

} // namespace avalang::ui
