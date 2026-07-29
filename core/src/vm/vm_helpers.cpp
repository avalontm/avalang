#include "vm.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <string>
#include "vm_helpers.h"
#include "vm_platform_accessor.h"

#ifdef _WIN32
#define PATH_SEPARATOR_CHAR '\\'
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR_CHAR '/'
#define PATH_SEPARATOR "/"
#endif

namespace ava {

std::string GetFileDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

std::string NumberToString(double n) {
    if (std::abs(n - std::round(n)) < 0.0000001)
        return std::to_string(static_cast<long long>(std::round(n)));
    std::ostringstream oss;
    oss << std::setprecision(15) << n;
    std::string s = oss.str();
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last_not_zero = s.find_last_not_of('0');
        if (last_not_zero == dot)
            s.erase(dot);
        else
            s.erase(last_not_zero + 1);
    }
    return s;
}

size_t ValidateIntegerIndex(double n, const char* context) {
    if (std::abs(n - std::round(n)) >= 0.0000001) {
        throw std::runtime_error(std::string(context) + ": index must be an integer, got " + NumberToString(n));
    }
    double rounded = std::round(n);
    if (rounded < 0) {
        throw std::runtime_error(std::string(context) + ": index must not be negative, got " + NumberToString(rounded));
    }
    if (rounded > static_cast<double>(SIZE_MAX)) {
        throw std::runtime_error(std::string(context) + ": index too large: " + NumberToString(rounded));
    }
    return static_cast<size_t>(rounded);
}

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = PATH_SEPARATOR_CHAR;
    if (a.back() == sep || a.back() == '/') return a + b;
    return a + PATH_SEPARATOR + b;
}

bool FileExists(const std::string& path) {
    return VmPlatformAccessor::Get().FileSystem().Exists(path);
}

std::string GetCurrentWorkingDir() {
    return VmPlatformAccessor::Get().Environment().GetCurrentDirectory();
}

// Static (non-instance) attrs live only on the class that declared them.
// CompileClass no longer copies a subclass's base attrs into its own
// `attrs` map (that made `Sub.x` and `Base.x` two independent copies as
// soon as either was written to) -- instead a subclass just carries a
// `__base__` link, and lookups walk it here to find the ClassObj that
// actually owns `name`, so `Base.x` and `Sub.x` stay the same storage.
ClassObj* FindClassOwningAttr(ClassObj* cls, const std::string& name) {
    ClassObj* cur = cls;
    while (cur) {
        if (cur->attrs.find(name) != cur->attrs.end()) return cur;
        auto base_it = cur->attrs.find("__base__");
        if (base_it == cur->attrs.end() || base_it->second.type != ValueType::Class) break;
        cur = static_cast<ClassObj*>(base_it->second.obj);
    }
    return nullptr;
}

} // namespace ava