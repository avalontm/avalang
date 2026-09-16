#include "perf/MemoryProfiler.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace perf {

MemoryProfiler& MemoryProfiler::Instance() {
    static MemoryProfiler instance;
    return instance;
}

void MemoryProfiler::Track(const std::string& tag, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    MemoryTagStats& stats = tags_[tag];
    stats.tag = tag;
    stats.currentBytes += bytes;
    stats.allocationCount += 1;
    stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);

    totalBytes_ += bytes;
}

void MemoryProfiler::Untrack(const std::string& tag, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tags_.find(tag);
    if (it == tags_.end()) {
        return;
    }

    MemoryTagStats& stats = it->second;
    bytes = std::min(bytes, stats.currentBytes);
    stats.currentBytes -= bytes;
    stats.freeCount += 1;

    bytes = std::min(bytes, totalBytes_);
    totalBytes_ -= bytes;
}

size_t MemoryProfiler::TotalBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalBytes_;
}

MemoryTagStats MemoryProfiler::TagStats(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tags_.find(tag);
    if (it == tags_.end()) {
        MemoryTagStats empty;
        empty.tag = tag;
        return empty;
    }
    return it->second;
}

std::vector<MemoryTagStats> MemoryProfiler::AllStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryTagStats> result;
    result.reserve(tags_.size());
    for (const auto& entry : tags_) {
        result.push_back(entry.second);
    }
    return result;
}

void MemoryProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tags_.clear();
    totalBytes_ = 0;
}

}
}
}
