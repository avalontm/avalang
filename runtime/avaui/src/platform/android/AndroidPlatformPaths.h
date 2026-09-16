#pragma once

#include "../contract/IPlatformPaths.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidPlatformPaths final : public IPlatformPaths {
public:
    std::string FontsDir() const override;
    std::string SystemDir() const override;
    std::string AppDataDir() const override;
};

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
