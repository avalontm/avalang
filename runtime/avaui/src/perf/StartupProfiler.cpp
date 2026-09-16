#include "perf/StartupProfiler.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace perf {

StartupProfiler::StartupProfiler() {
    epoch_ = std::chrono::steady_clock::now();
}

StartupProfiler& StartupProfiler::Instance() {
    static StartupProfiler instance;
    return instance;
}

void StartupProfiler::BeginStage(const std::string& stage) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(PendingStage{stage, std::chrono::steady_clock::now()});
}

void StartupProfiler::EndStage(const std::string& stage) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) {
        if (it->stage != stage) {
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> duration = now - it->start;
        std::chrono::duration<double, std::milli> offset = it->start - epoch_;

        StartupStageTiming timing;
        timing.stage = stage;
        timing.durationMs = duration.count();
        timing.startOffsetMs = static_cast<uint64_t>(offset.count());
        timings_.push_back(timing);

        pending_.erase(std::next(it).base());
        return;
    }
}

std::vector<StartupStageTiming> StartupProfiler::Timings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timings_;
}

double StartupProfiler::TotalMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto& timing : timings_) {
        total += timing.durationMs;
    }
    return total;
}

void StartupProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.clear();
    timings_.clear();
    epoch_ = std::chrono::steady_clock::now();
}

ScopedStartupStage::ScopedStartupStage(std::string stage) : stage_(std::move(stage)) {
    StartupProfiler::Instance().BeginStage(stage_);
}

ScopedStartupStage::~ScopedStartupStage() {
    StartupProfiler::Instance().EndStage(stage_);
}

}
}
}
