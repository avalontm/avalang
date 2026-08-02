#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace avahost {

struct CacheEntry {
    std::string html;
    std::string stateJson;
    uint64_t contentHash;
};

class UiBytecodeCache {
public:
    UiBytecodeCache() = default;
    ~UiBytecodeCache() = default;

    // Try to get cached render result by file path and content hash.
    // Returns true if found and valid (hash matches).
    bool Get(const std::string& filePath, uint64_t contentHash,
             std::string& outHtml, std::string& outStateJson);

    // Store render result in cache.
    void Set(const std::string& filePath, uint64_t contentHash,
             const std::string& html, const std::string& stateJson);

    // Invalidate cache entry for a single file.
    void Invalidate(const std::string& filePath);

    // Clear entire cache (e.g., on hot-reload).
    void Clear();

    // Get cache statistics.
    size_t Size() const;

private:
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex mutex_;
};

} // namespace avahost
