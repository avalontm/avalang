#pragma once

#include "platform/contract/IPlatformPaths.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinPlatformPaths final : public IPlatformPaths {
public:
    std::string FontsDir() const override;
    std::string SystemDir() const override;
    std::string AppDataDir() const override;
};

}
}
}
}
