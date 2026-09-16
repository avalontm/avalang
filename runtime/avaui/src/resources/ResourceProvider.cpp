#include "resources/ResourceProvider.h"
#include "platform/contract/IPlatform.h"
#include "perf/MemoryProfiler.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace avalang {
namespace ui {

ResourceProvider::ResourceProvider() {
    InitializeDefaultPrefixes();
}

ResourceProvider::~ResourceProvider() {
    ClearCache();
}

std::string ResourceProvider::Normalize(const std::string& path) {
    std::string result = path;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

void ResourceProvider::InitializeDefaultPrefixes() {
    platform::IPlatformPaths& paths = platform::GetPlatform().Paths();

    std::string fontsDir = paths.FontsDir();
    if (!fontsDir.empty()) {
        RegisterPrefix("@fonts", fontsDir);
    }

    std::string systemDir = paths.SystemDir();
    if (!systemDir.empty()) {
        RegisterPrefix("@system", systemDir);
    }

    std::string appDataDir = paths.AppDataDir();
    if (!appDataDir.empty()) {
        RegisterPrefix("@appdata", appDataDir);
    }

    RegisterPrefix("@local", ".");
    RegisterPrefix("@root", ".");
}

bool ResourceProvider::RegisterPrefix(const std::string& prefix, const std::string& physicalPath) {
    if (prefix.empty() || physicalPath.empty()) {
        return false;
    }
    std::string normPrefix = Normalize(prefix);
    prefixes_[normPrefix] = PrefixEntry{physicalPath};
    return true;
}

std::string ResourceProvider::FindFileWithExtensions(
    const std::string& baseDir,
    const std::string& baseName,
    ResourceType type
) {
    std::vector<const char*> extensions;

    switch (type) {
        case ResourceType::Font:
            extensions = {".ttf", ".otf", ".fon"};
            break;
        case ResourceType::Image:
        case ResourceType::Icon:
            extensions = {".png", ".bmp", ".jpg", ".jpeg", ".gif"};
            break;
        case ResourceType::Localization:
            extensions = {".lang", ".txt", ""};
            break;
        default:
            extensions = {""};
    }

    for (const char* ext : extensions) {
        std::string filePath = baseDir + "/" + baseName + ext;

        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            return filePath;
        }
    }

    return {};
}

bool ResourceProvider::SplitLogicalPath(const std::string& normPath, std::string& outPrefixDir, std::string& outName) {
    size_t slashPos = normPath.find('/');
    if (slashPos == std::string::npos) {
        return false;
    }

    std::string prefix = normPath.substr(0, slashPos);
    outName = normPath.substr(slashPos + 1);

    auto prefixIt = prefixes_.find(prefix);
    if (prefixIt == prefixes_.end()) {
        return false;
    }

    outPrefixDir = prefixIt->second.physicalPath;
    return true;
}

std::unique_ptr<std::vector<uint8_t>> ResourceProvider::TryLoadWithExtensions(
    const std::string& baseDir,
    const std::string& baseName,
    ResourceType type
) {
    std::string filePath = FindFileWithExtensions(baseDir, baseName, type);
    if (filePath.empty()) {
        return nullptr;
    }

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return nullptr;

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    auto buffer = std::make_unique<std::vector<uint8_t>>(fileSize);
    file.read(reinterpret_cast<char*>(buffer->data()), fileSize);

    if (file.gcount() == static_cast<std::streamsize>(fileSize)) {
        return buffer;
    }

    return nullptr;
}

bool ResourceProvider::LoadImageMetadata(const std::string& filePath, ResourceMetadata& out) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    uint16_t magic;
    file.read(reinterpret_cast<char*>(&magic), 2);

    if (magic == 0x4D42) {
        file.seekg(18);
        uint32_t width, height;
        file.read(reinterpret_cast<char*>(&width), 4);
        file.read(reinterpret_cast<char*>(&height), 4);

        out.width = width;
        out.height = height;
        out.stride = ((width * 24 + 31) / 32) * 4;

        file.seekg(0, std::ios::end);
        out.dataSize = static_cast<uint32_t>(file.tellg());
        return true;
    }

    if (magic == 0x8950) {
        file.seekg(16);
        uint32_t width, height;
        uint8_t w[4], h[4];
        file.read(reinterpret_cast<char*>(w), 4);
        file.read(reinterpret_cast<char*>(h), 4);

        width = (w[0] << 24) | (w[1] << 16) | (w[2] << 8) | w[3];
        height = (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3];

        out.width = width;
        out.height = height;
        out.stride = width * 4;

        file.seekg(0, std::ios::end);
        out.dataSize = static_cast<uint32_t>(file.tellg());
        return true;
    }

    std::ifstream fallbackFile(filePath, std::ios::binary | std::ios::ate);
    if (fallbackFile.is_open()) {
        out.dataSize = static_cast<uint32_t>(fallbackFile.tellg());
        out.width = 0;
        out.height = 0;
        out.stride = 0;
        return true;
    }

    return false;
}

void ResourceProvider::TouchLru(const std::string& key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return;

    lruOrder_.erase(it->second.lruIt);
    lruOrder_.push_front(key);
    it->second.lruIt = lruOrder_.begin();
}

void ResourceProvider::InsertIntoCache(const std::string& key, CachedResource&& resource) {
    size_t bytes = resource.buffer.size();

    lruOrder_.push_front(key);
    resource.lruIt = lruOrder_.begin();
    cacheBytes_ += bytes;
    perf::MemoryProfiler::Instance().Track("ResourceProvider", bytes);

    cache_[key] = std::move(resource);

    EvictUntilWithinBudget();
}

void ResourceProvider::EvictUntilWithinBudget() {
    while (cacheBytes_ > maxCacheBytes_ && !lruOrder_.empty()) {
        std::string lruKey = lruOrder_.back();
        auto it = cache_.find(lruKey);
        if (it != cache_.end()) {
            size_t bytes = it->second.buffer.size();
            cacheBytes_ -= bytes;
            perf::MemoryProfiler::Instance().Untrack("ResourceProvider", bytes);
            cache_.erase(it);
            stats_.evictions += 1;
        }
        lruOrder_.pop_back();
    }
}

Resource ResourceProvider::Load(const std::string& logicalPath, ResourceType type) {
    std::string normPath = Normalize(logicalPath);

    auto cacheIt = cache_.find(normPath);
    if (cacheIt != cache_.end()) {
        stats_.hits += 1;
        TouchLru(normPath);
        return Resource(cacheIt->second.buffer.data(), cacheIt->second.metadata);
    }
    stats_.misses += 1;

    std::string prefixDir;
    std::string name;
    if (!SplitLogicalPath(normPath, prefixDir, name)) {
        return Resource();
    }

    auto buffer = TryLoadWithExtensions(prefixDir, name, type);
    if (!buffer) {
        return Resource();
    }

    ResourceMetadata metadata;
    metadata.type = type;
    metadata.dataSize = buffer->size();

    if (type == ResourceType::Image || type == ResourceType::Icon) {
        std::string fullPath = FindFileWithExtensions(prefixDir, name, type);
        LoadImageMetadata(fullPath, metadata);
    }

    CachedResource cached;
    cached.buffer = std::move(*buffer);
    cached.metadata = metadata;

    const uint8_t* dataPtr = cached.buffer.data();
    InsertIntoCache(normPath, std::move(cached));

    return Resource(dataPtr, metadata);
}

std::string ResourceProvider::ResolvePhysicalPath(const std::string& logicalPath, ResourceType type) {
    std::string normPath = Normalize(logicalPath);

    std::string prefixDir;
    std::string name;
    if (!SplitLogicalPath(normPath, prefixDir, name)) {
        return {};
    }

    return FindFileWithExtensions(prefixDir, name, type);
}

bool ResourceProvider::Exists(const std::string& logicalPath) {
    std::string normPath = Normalize(logicalPath);

    if (cache_.find(normPath) != cache_.end()) {
        return true;
    }

    Resource res = Load(logicalPath, ResourceType::Unknown);
    return res.data != nullptr;
}

void ResourceProvider::ClearCache() {
    for (const auto& entry : cache_) {
        perf::MemoryProfiler::Instance().Untrack("ResourceProvider", entry.second.buffer.size());
    }
    cache_.clear();
    lruOrder_.clear();
    cacheBytes_ = 0;
}

void ResourceProvider::SetMaxCacheBytes(size_t maxBytes) {
    maxCacheBytes_ = maxBytes;
    EvictUntilWithinBudget();
}

ResourceCacheStats ResourceProvider::CacheStats() const {
    ResourceCacheStats stats = stats_;
    stats.currentBytes = cacheBytes_;
    stats.maxBytes = maxCacheBytes_;
    stats.entryCount = cache_.size();
    return stats;
}

IResourceProvider* CreateDefaultResourceProvider() {
    return new ResourceProvider();
}

}
}