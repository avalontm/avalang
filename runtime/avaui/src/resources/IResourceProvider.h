#pragma once

#include "Export.h"
#include <cstdint>
#include <string>
#include <cstdint>
#include <memory>

namespace avalang::ui {

/**
 * Resource types supported by the UI system.
 */
enum class ResourceType {
    Unknown = 0,
    Font = 1,       // TrueType, OpenType fonts (.ttf, .otf)
    Image = 2,      // Raster images (.bmp, .png, .jpg, etc.)
    Icon = 3,       // Icons (potentially scalable or atlased)
    Localization = 4, // Language strings / translation tables
};

/**
 * Metadata about a loaded resource.
 */
struct ResourceMetadata {
    ResourceType type = ResourceType::Unknown;
    uint32_t width = 0;    // For images/icons: pixel width
    uint32_t height = 0;   // For images/icons: pixel height
    uint32_t stride = 0;   // For images: bytes per row (row-major)
    uint32_t dataSize = 0; // Total byte size of the resource data
};

/**
 * Represents a loaded resource in memory.
 * The buffer ownership is managed by the IResourceProvider.
 */
struct Resource {
    const uint8_t* data = nullptr;
    ResourceMetadata metadata;
    
    Resource() = default;
    Resource(const uint8_t* d, const ResourceMetadata& m) 
        : data(d), metadata(m) {}
};

/**
 * Interface for resolving and loading UI resources (fonts, images, icons, localizations).
 * 
 * Implementations handle:
 * - Logical path resolution (e.g., "@fonts/Arial" -> "C:\...\Arial.ttf")
 * - Resource loading from filesystem or embedded
 * - Optional caching to avoid repeated disk I/O
 * - Error handling for missing or corrupt resources
 * 
 * All paths are case-insensitive (normalized to lowercase internally).
 */
class IResourceProvider {
public:
    virtual ~IResourceProvider() = default;

    /**
     * Attempt to load a resource by logical path.
     * 
     * @param logicalPath Logical resource path (e.g., "@fonts/Segoe_UI", "@icons/check")
     * @param type Expected resource type (optimization hint; can be Unknown)
     * @return Resource struct with data pointer and metadata, or empty if not found
     */
    virtual Resource Load(const std::string& logicalPath, ResourceType type) = 0;

    /**
     * Register a prefix mapping for resource resolution.
     * 
     * Example: RegisterPrefix("@fonts", "C:\\Windows\\Fonts")
     * Then "@fonts/Arial" resolves by looking for Arial.* in that directory.
     * 
     * @param prefix Logical prefix (e.g., "@fonts", "@icons")
     * @param physicalPath Physical filesystem or archive path
     * @return true if registration succeeded
     */
    virtual bool RegisterPrefix(const std::string& prefix, const std::string& physicalPath) = 0;

    /**
     * Check if a resource exists without loading it.
     * 
     * @param logicalPath Resource logical path
     * @return true if resource can be found
     */
    virtual bool Exists(const std::string& logicalPath) = 0;

    /**
     * Clear all cached resources to free memory.
     * Subsequent Load() calls will re-fetch from disk.
     */
    virtual void ClearCache() = 0;

    /**
     * Get ABI version for binary compatibility.
     */
    virtual uint32_t AbiVersion() const = 0;
};

/**
 * Factory function to create a default IResourceProvider.
 * On Windows: filesystem-based provider with standard system paths.
 * On Linux/macOS: stub returning empty resources (TODO).
 * 
 * @return Heap-allocated IResourceProvider; caller must delete
 */
AVA_UI_API IResourceProvider* CreateDefaultResourceProvider();

} // namespace avalang::ui
