#pragma once

#include "IResourceProvider.h"
#include <list>
#include <unordered_map>
#include <memory>
#include <vector>

namespace avalang {
namespace ui {

class ResourceProvider : public IResourceProvider {
public:
    ResourceProvider();
    virtual ~ResourceProvider();

    Resource Load(const std::string& logicalPath, ResourceType type) override;
    std::string ResolvePhysicalPath(const std::string& logicalPath, ResourceType type) override;
    bool RegisterPrefix(const std::string& prefix, const std::string& physicalPath) override;
    bool Exists(const std::string& logicalPath) override;
    void ClearCache() override;
    void SetMaxCacheBytes(size_t maxBytes) override;
    ResourceCacheStats CacheStats() const override;
    uint32_t AbiVersion() const override { return 17; }

private:
    struct CachedResource {
        std::vector<uint8_t> buffer;
        ResourceMetadata metadata;
        std::list<std::string>::iterator lruIt;
    };

    struct PrefixEntry {
        std::string physicalPath;
    };

    std::unordered_map<std::string, CachedResource> cache_;
    std::unordered_map<std::string, PrefixEntry> prefixes_;

    std::list<std::string> lruOrder_;
    size_t cacheBytes_ = 0;
    size_t maxCacheBytes_ = 64u * 1024u * 1024u;
    ResourceCacheStats stats_;

    void TouchLru(const std::string& key);
    void InsertIntoCache(const std::string& key, CachedResource&& resource);
    void EvictUntilWithinBudget();

    static std::string Normalize(const std::string& path);

    std::unique_ptr<std::vector<uint8_t>> TryLoadWithExtensions(
        const std::string& baseDir,
        const std::string& baseName,
        ResourceType type
    );

    std::string FindFileWithExtensions(
        const std::string& baseDir,
        const std::string& baseName,
        ResourceType type
    );

    bool SplitLogicalPath(const std::string& normPath, std::string& outPrefixDir, std::string& outName);

    void InitializeDefaultPrefixes();

    bool LoadImageMetadata(const std::string& filePath, ResourceMetadata& out);
};

}
}