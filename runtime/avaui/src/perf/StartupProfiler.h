#pragma once

#include "Export.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace perf {

struct StartupStageTiming {
    std::string stage;
    double durationMs = 0.0;
    uint64_t startOffsetMs = 0;
};

class AVA_UI_API StartupProfiler {
public:
    static StartupProfiler& Instance();

    void BeginStage(const std::string& stage);
    void EndStage(const std::string& stage);

    std::vector<StartupStageTiming> Timings() const;
    double TotalMs() const;

    void Reset();

private:
    StartupProfiler();

    struct PendingStage {
        std::string stage;
        std::chrono::steady_clock::time_point start;
    };

    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point epoch_;
    std::vector<PendingStage> pending_;
    std::vector<StartupStageTiming> timings_;
};

class AVA_UI_API ScopedStartupStage {
public:
    explicit ScopedStartupStage(std::string stage);
    ~ScopedStartupStage();

    ScopedStartupStage(const ScopedStartupStage&) = delete;
    ScopedStartupStage& operator=(const ScopedStartupStage&) = delete;

private:
    std::string stage_;
};

}
}
}
