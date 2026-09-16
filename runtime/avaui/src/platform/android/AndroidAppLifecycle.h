#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/IAppLifecycle.h"

#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidAppLifecycle final : public ava::platform::mobile::IAppLifecycle {
public:
    void AddObserver(ava::platform::mobile::IAppLifecycleObserver* observer) override;
    void RemoveObserver(ava::platform::mobile::IAppLifecycleObserver* observer) override;
    ava::platform::mobile::AppLifecycleState CurrentState() const override;

    void SetState(ava::platform::mobile::AppLifecycleState state);
    void NotifyLowMemory();

private:
    std::vector<ava::platform::mobile::IAppLifecycleObserver*> observers_;
    ava::platform::mobile::AppLifecycleState state_ = ava::platform::mobile::AppLifecycleState::Created;
};

AndroidAppLifecycle& GetAppLifecycle();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
