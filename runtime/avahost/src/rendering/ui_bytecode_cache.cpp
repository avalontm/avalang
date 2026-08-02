#include "ui_bytecode_cache.h"

namespace avahost {

bool UiBytecodeCache::Get(const std::string& filePath, uint64_t contentHash,
                          std::string& outHtml, std::string& outStateJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(filePath);
    if (it != cache_.end() && it->second.contentHash == contentHash) {
        outHtml = it->second.html;
        outStateJson = it->second.stateJson;
        return true;
    }
    return false;
}

void UiBytecodeCache::Set(const std::string& filePath, uint64_t contentHash,
                          const std::string& html, const std::string& stateJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[filePath] = CacheEntry{html, stateJson, contentHash};
}

void UiBytecodeCache::Invalidate(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(filePath);
}

void UiBytecodeCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

size_t UiBytecodeCache::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

} // namespace avahost
