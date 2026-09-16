#include "resources/ResourcePathResolver.h"

namespace avalang {
namespace ui {
namespace resources {

bool HasLogicalPrefix(const std::string& path) {
    return !path.empty() && path[0] == '@';
}

std::string ResolveResourcePath(const std::string& logicalPath, ResourceBackend backend) {
    if (!HasLogicalPrefix(logicalPath)) {
        return logicalPath;
    }

    size_t slashPos = logicalPath.find('/');
    if (slashPos == std::string::npos) {
        return logicalPath;
    }
    std::string prefix = logicalPath.substr(0, slashPos);
    std::string rest = logicalPath.substr(slashPos + 1);

    switch (backend) {
        case ResourceBackend::Web:
            if (prefix == "@local" || prefix == "@root") {
                return "/" + rest;
            }
            if (prefix == "@icons") {
                return "/icons/" + rest;
            }
            if (prefix == "@fonts") {
                return "/fonts/" + rest;
            }
            return logicalPath;

        case ResourceBackend::Desktop:
        default:
            return logicalPath;
    }
}

}
}
}