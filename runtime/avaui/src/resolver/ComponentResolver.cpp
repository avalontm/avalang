#include "resolver/ComponentResolver.h"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "components/PropertyValue.h"
#include "resolver/DottedPath.h"
#include "resolver/KnownComponentProperties.h"

namespace fs = std::filesystem;

namespace avalang {
namespace ui {

namespace {

bool ReadFile(const std::string& path, std::string& outContents) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream contents;
    contents << file.rdbuf();
    outContents = contents.str();
    return true;
}

} // namespace

ComponentResolver::ComponentResolver(std::string projectRoot, std::string componentsDir)
    : projectRoot_(std::move(projectRoot)), componentsDir_(std::move(componentsDir)) {}

bool ComponentResolver::IsComponentCall(const IComponent* comp) {
    if (!comp) return false;
    if (const auto* prop = comp->GetProperty("__unresolvedImportCall")) {
        if (prop->Type() == PropertyType::Bool && prop->AsBool()) {
            return true;
        }
    }
    return false;
}

void ComponentResolver::MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                                       const std::unordered_map<std::string, std::string>& addition) {
    for (const auto& [k, v] : addition) {
        if (merged.find(k) == merged.end()) {
            merged[k] = v;
        }
    }
}

ComponentResolver::ImportMap ComponentResolver::BuildImportMap(const std::vector<std::string>& imports) const {
    ImportMap map;
    for (const auto& dotted : imports) {
        std::string tag = CallableTagFromDotted(dotted);
        if (tag.empty()) continue;
        fs::path resolved = ResolveDottedAvauiPath(projectRoot_, dotted);
        map[tag] = ImportMapEntry{dotted, resolved.string()};
    }
    return map;
}

const ComponentResolver::CacheEntry* ComponentResolver::LoadComponent(
        const std::string& typeName,
        std::unordered_map<std::string, std::string>& mergedState,
        const ImportMap& importMap) {
    fs::path filePath;
    auto it = importMap.find(typeName);
    if (it != importMap.end()) {
        filePath = it->second.resolvedFilePath;
    } else {
        filePath = fs::path(componentsDir_) / (typeName + ".avaui");
    }

    std::error_code ec;
    auto writeTime = fs::last_write_time(filePath, ec);
    if (ec) return nullptr;

    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t mtime = std::chrono::system_clock::to_time_t(sctp);

    std::string key = filePath.string();
    auto cacheIt = cache_.find(key);
    if (cacheIt != cache_.end() && cacheIt->second->mtime == mtime) {
        MergeStateMap(mergedState, cacheIt->second->parsed.state);
        return cacheIt->second.get();
    }

    std::string source;
    if (!ReadFile(key, source)) return nullptr;

    parser::ParsedAvaui parsed;
    try {
        parsed = parser::AvauiParser::Parse(source);
    } catch (const parser::ParseError&) {
        return nullptr;
    }
    if (!parsed.tree) return nullptr;

    auto entry = std::make_unique<CacheEntry>();
    entry->mtime = mtime;
    entry->parsed = std::move(parsed);

    ImportMap ownImportMap = BuildImportMap(entry->parsed.imports);
    IComponent* root = entry->parsed.tree->Root();
    if (root) {
        ResolveChildrenOf(root, entry->parsed.tree.get(), mergedState, ownImportMap, 0);
    }
    MergeStateMap(mergedState, entry->parsed.state);

    CacheEntry* raw = entry.get();
    cache_[key] = std::move(entry);
    return raw;
}

void ComponentResolver::ResolveChildrenOf(IComponent* parent,
                                           ComponentTree* tree,
                                           std::unordered_map<std::string, std::string>& mergedState,
                                           const ImportMap& importMap, int depth) {
    if (!parent || !tree || depth > kMaxDepth) return;

    auto slotNames = parent->SlotNames();
    for (const auto& slot : slotNames) {
        const auto& children = parent->SlotChildren(slot);
        std::vector<IComponent*> originals(children.begin(), children.end());
        std::vector<IComponent*> news;
        news.reserve(originals.size());

        bool changed = false;
        for (IComponent* child : originals) {
            if (IsComponentCall(child)) {
                std::string typeName = child->TypeName();
                const CacheEntry* comp = LoadComponent(typeName, mergedState, importMap);
                if (comp && comp->parsed.tree) {
                    IComponent* compRoot = comp->parsed.tree->Root();
                    if (compRoot) {
                        for (const auto& srcChild : compRoot->Children()) {
                            IComponent* cloned = CloneInto(srcChild, nullptr, tree);
                            if (cloned) {
                                ApplyCallSiteOverrides(child, cloned);
                                news.push_back(cloned);
                            }
                        }
                    }
                    changed = true;
                    continue;
                }
            }
            news.push_back(child);
        }

        if (changed) {
            for (IComponent* c : originals) parent->RemoveChild(c);
            for (IComponent* c : news) parent->AddChild(c, slot);
        }

        for (IComponent* c : news) {
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1);
        }
    }
}

IComponent* ComponentResolver::CloneInto(const IComponent* src,
                                          IComponent* parent,
                                          ComponentTree* dst) {
    if (!src || !dst) return nullptr;
    IComponent* clone = dst->CreateComponent(src->TypeName());
    if (!clone) return nullptr;

    for (const auto& name : src->PropertyNames()) {
        if (const auto* p = src->GetProperty(name)) {
            clone->SetProperty(name, *p);
        }
    }

    if (parent) parent->AddChild(clone);

    for (const auto& child : src->Children()) {
        CloneInto(child, clone, dst);
    }
    return clone;
}

void ComponentResolver::ApplyCallSiteOverrides(const IComponent* callSite,
                                                  IComponent* clonedRoot) {
    if (!callSite || !clonedRoot) return;

    for (const auto& name : callSite->PropertyNames()) {
        if (const auto* p = callSite->GetProperty(name)) {
            clonedRoot->SetProperty(name, *p);
        }
    }
}

void ComponentResolver::ResolveImports(ComponentTree* tree,
                                          const std::vector<std::string>& imports,
                                          std::unordered_map<std::string, std::string>& mergedState) {
    if (!tree) return;
    IComponent* root = tree->Root();
    if (!root) return;

    ImportMap importMap = BuildImportMap(imports);
    ResolveChildrenOf(root, tree, mergedState, importMap, 0);
}

} // namespace ui
} // namespace avalang
