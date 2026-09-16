#pragma once

#include "Export.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang {
namespace ui {
namespace perf {

struct MemoryTagStats {
    std::string tag;
    size_t currentBytes = 0;
    size_t peakBytes = 0;
    uint64_t allocationCount = 0;
    uint64_t freeCount = 0;
};

class AVA_UI_API MemoryProfiler {
public:
    static MemoryProfiler& Instance();

    void Track(const std::string& tag, size_t bytes);
    void Untrack(const std::string& tag, size_t bytes);

    size_t TotalBytes() const;
    MemoryTagStats TagStats(const std::string& tag) const;
    std::vector<MemoryTagStats> AllStats() const;

    void Reset();

private:
    MemoryProfiler() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MemoryTagStats> tags_;
    size_t totalBytes_ = 0;
};

}
}
}
