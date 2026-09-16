#pragma once

#include <string>

namespace avalang {
namespace ui {
namespace platform {

class IPlatformPaths {
public:
    virtual ~IPlatformPaths() = default;

    virtual std::string FontsDir() const = 0;
    virtual std::string SystemDir() const = 0;
    virtual std::string AppDataDir() const = 0;
};

}
}
}
