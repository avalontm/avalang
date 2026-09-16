#pragma once

#include "Export.h"
#include <cstdint>
#include <string>
#include <memory>

namespace avalang {
namespace ui {

enum class ResourceType {
    Unknown = 0,
    Font = 1,
    Image = 2,
    Icon = 3,
    Localization = 4,
};

struct ResourceMetadata {
    ResourceType type = ResourceType::Unknown;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t dataSize = 0;
};

struct Resource {
    const uint8_t* data = nullptr;
    ResourceMetadata metadata;

    Resource() = default;
    Resource(const uint8_t* d, const ResourceMetadata& m)
        : data(d), metadata(m) {}
};

struct ResourceCacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    size_t currentBytes = 0;
    size_t maxBytes = 0;
    size_t entryCount = 0;
};

class IResourceProvider {
public:
    virtual ~IResourceProvider() = default;

    virtual Resource Load(const std::string& logicalPath, ResourceType type) = 0;

    virtual std::string ResolvePhysicalPath(const std::string& logicalPath, ResourceType type) = 0;

    virtual bool RegisterPrefix(const std::string& prefix, const std::string& physicalPath) = 0;

    virtual bool Exists(const std::string& logicalPath) = 0;

    virtual void ClearCache() = 0;

    virtual void SetMaxCacheBytes(size_t maxBytes) = 0;
    virtual ResourceCacheStats CacheStats() const = 0;

    virtual uint32_t AbiVersion() const = 0;
};

AVA_UI_API IResourceProvider* CreateDefaultResourceProvider();

}
}