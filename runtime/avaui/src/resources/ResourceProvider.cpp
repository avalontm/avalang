#include "resources/ResourceProvider.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shlobj.h>
    #pragma comment(lib, "shell32.lib")
#else
    // Linux/macOS: stub
#endif

namespace avalang::ui {

ResourceProvider::ResourceProvider() {
    InitializeDefaultPrefixes();
}

ResourceProvider::~ResourceProvider() {
    ClearCache();
}

std::string ResourceProvider::Normalize(const std::string& path) {
    std::string result = path;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

void ResourceProvider::InitializeDefaultPrefixes() {
#ifdef _WIN32
    // Standard Windows system directories
    char sysPath[MAX_PATH];
    GetWindowsDirectoryA(sysPath, sizeof(sysPath));
    
    std::string windir(sysPath);
    RegisterPrefix("@fonts", windir + "\\Fonts");
    RegisterPrefix("@system", windir + "\\System32");
    
    // Common app data paths
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        RegisterPrefix("@appdata", appdata);
    }
    
    // Current directory for local resources
    RegisterPrefix("@local", ".");
    RegisterPrefix("@root", ".");
#else
    // Linux/macOS: minimal defaults
    RegisterPrefix("@fonts", "/usr/share/fonts");
    RegisterPrefix("@local", ".");
    RegisterPrefix("@root", ".");
#endif
}

bool ResourceProvider::RegisterPrefix(const std::string& prefix, const std::string& physicalPath) {
    if (prefix.empty() || physicalPath.empty()) {
        return false;
    }
    std::string normPrefix = Normalize(prefix);
    prefixes_[normPrefix] = PrefixEntry{physicalPath};
    return true;
}

std::unique_ptr<std::vector<uint8_t>> ResourceProvider::TryLoadWithExtensions(
    const std::string& baseDir,
    const std::string& baseName,
    ResourceType type
) {
    std::vector<const char*> extensions;
    
    switch (type) {
        case ResourceType::Font:
            extensions = {".ttf", ".otf", ".fon"};
            break;
        case ResourceType::Image:
        case ResourceType::Icon:
            extensions = {".png", ".bmp", ".jpg", ".jpeg", ".gif"};
            break;
        default:
            extensions = {""}; // Try without extension
    }

    for (const char* ext : extensions) {
        std::string filePath = baseDir + "/" + baseName + ext;
        
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        auto buffer = std::make_unique<std::vector<uint8_t>>(fileSize);
        file.read(reinterpret_cast<char*>(buffer->data()), fileSize);
        
        if (file.gcount() == static_cast<std::streamsize>(fileSize)) {
            return buffer;
        }
    }

    return nullptr;
}

bool ResourceProvider::LoadImageMetadata(const std::string& filePath, ResourceMetadata& out) {
    // Basic image header detection (Windows only for Phase 15)
#ifdef _WIN32
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    uint16_t magic;
    file.read(reinterpret_cast<char*>(&magic), 2);

    // BMP magic: 0x4D42 ('BM')
    if (magic == 0x4D42) {
        file.seekg(18);
        uint32_t width, height;
        file.read(reinterpret_cast<char*>(&width), 4);
        file.read(reinterpret_cast<char*>(&height), 4);
        
        out.width = width;
        out.height = height;
        out.stride = ((width * 24 + 31) / 32) * 4; // 24-bit BMP standard stride
        
        file.seekg(0, std::ios::end);
        out.dataSize = static_cast<uint32_t>(file.tellg());
        return true;
    }
    
    // PNG magic: 0x89504E47 (89 50 4E 47)
    if (magic == 0x8950) {
        file.seekg(16);
        uint32_t width, height;
        // PNG stores big-endian
        uint8_t w[4], h[4];
        file.read(reinterpret_cast<char*>(w), 4);
        file.read(reinterpret_cast<char*>(h), 4);
        
        width = (w[0] << 24) | (w[1] << 16) | (w[2] << 8) | w[3];
        height = (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3];
        
        out.width = width;
        out.height = height;
        out.stride = width * 4; // Assume RGBA for now
        
        file.seekg(0, std::ios::end);
        out.dataSize = static_cast<uint32_t>(file.tellg());
        return true;
    }
#endif
    
    // Fallback: just return file size as metadata
    std::ifstream fallbackFile(filePath, std::ios::binary | std::ios::ate);
    if (fallbackFile.is_open()) {
        out.dataSize = static_cast<uint32_t>(fallbackFile.tellg());
        out.width = 0;
        out.height = 0;
        out.stride = 0;
        return true;
    }
    
    return false;
}

Resource ResourceProvider::Load(const std::string& logicalPath, ResourceType type) {
    std::string normPath = Normalize(logicalPath);

    // Check cache first
    auto cacheIt = cache_.find(normPath);
    if (cacheIt != cache_.end()) {
        return Resource(cacheIt->second.buffer.data(), cacheIt->second.metadata);
    }

    // Parse logical path: @prefix/name
    size_t slashPos = normPath.find('/');
    if (slashPos == std::string::npos) {
        return Resource(); // Invalid path
    }

    std::string prefix = normPath.substr(0, slashPos);
    std::string name = normPath.substr(slashPos + 1);

    // Look up prefix
    auto prefixIt = prefixes_.find(prefix);
    if (prefixIt == prefixes_.end()) {
        return Resource(); // Unknown prefix
    }

    // Try to load file
    auto buffer = TryLoadWithExtensions(prefixIt->second.physicalPath, name, type);
    if (!buffer) {
        return Resource(); // File not found
    }

    // Load metadata for images
    ResourceMetadata metadata;
    metadata.type = type;
    metadata.dataSize = buffer->size();

    if (type == ResourceType::Image || type == ResourceType::Icon) {
        std::string fullPath = prefixIt->second.physicalPath + "/" + name;
        LoadImageMetadata(fullPath, metadata);
    }

    // Cache and return
    CachedResource cached;
    cached.buffer = std::move(*buffer);
    cached.metadata = metadata;

    const uint8_t* dataPtr = cached.buffer.data();
    cache_[normPath] = std::move(cached);

    return Resource(dataPtr, metadata);
}

bool ResourceProvider::Exists(const std::string& logicalPath) {
    std::string normPath = Normalize(logicalPath);
    
    // Check cache
    if (cache_.find(normPath) != cache_.end()) {
        return true;
    }

    // Try to load (don't cache if just checking existence)
    Resource res = Load(logicalPath, ResourceType::Unknown);
    return res.data != nullptr;
}

void ResourceProvider::ClearCache() {
    cache_.clear();
}

IResourceProvider* CreateDefaultResourceProvider() {
    return new ResourceProvider();
}

} // namespace avalang::ui
