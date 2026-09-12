#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "languages/class_index.h"
#include "languages/function_index.h"

namespace studio {

class WorkspaceIndex {
public:
    struct Snapshot {
        ClassIndex classes;
        FunctionIndex functions;
    };

    WorkspaceIndex() = default;
    ~WorkspaceIndex();

    WorkspaceIndex(const WorkspaceIndex&) = delete;
    WorkspaceIndex& operator=(const WorkspaceIndex&) = delete;

    void SetRoot(const std::string& root_dir, const std::string& stdlib_dir);

    void MarkDirty();

    void Poll();

    std::shared_ptr<const Snapshot> CurrentSnapshot() const;

private:
    void JoinWorker();
    void StartRebuild();

    mutable std::mutex snapshot_mutex_;
    std::shared_ptr<const Snapshot> snapshot_ = std::make_shared<const Snapshot>();

    std::mutex config_mutex_;
    std::string root_dir_;
    std::string stdlib_dir_;

    std::thread worker_;
    std::atomic<bool> rebuild_pending_{false};
    std::atomic<bool> rebuild_running_{false};
};

}
