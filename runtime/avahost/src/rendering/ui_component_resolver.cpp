#include "ui_component_resolver.h"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "component/dotted_path.h"
#include "components/PropertyValue.h"
#include "known_component_properties.h"

namespace fs = std::filesystem;

namespace avahost {

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

UiComponentResolver::UiComponentResolver(std::string projectRoot, std::string componentsDir)
    : projectRoot_(std::move(projectRoot)), componentsDir_(std::move(componentsDir)) {}

bool UiComponentResolver::IsComponentCall(const avalang::ui::IComponent* comp) {
    if (!comp) return false;
    if (const auto* prop = comp->GetProperty("__unresolvedImportCall")) {
        if (prop->Type() == avalang::ui::PropertyType::Bool && prop->AsBool()) {
            return true;
        }
    }
    return false;
}

void UiComponentResolver::MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                                         const std::unordered_map<std::string, std::string>& addition) {
    for (const auto& [k, v] : addition) {
        if (merged.find(k) == merged.end()) {
            merged[k] = v;
        }
    }
}

UiComponentResolver::ImportMap UiComponentResolver::BuildImportMap(const std::vector<std::string>& imports) const {
    ImportMap map;
    for (const auto& dotted : imports) {
        std::string tag = CallableTagFromDotted(dotted);
        if (tag.empty()) continue;
        fs::path resolved = ResolveDottedAvauiPath(projectRoot_, dotted);
        map[tag] = ImportMapEntry{dotted, resolved.string()};
    }
    return map;
}

const UiComponentResolver::CacheEntry* UiComponentResolver::LoadComponent(
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

    avalang::ui::parser::ParsedAvaui parsed;
    try {
        parsed = avalang::ui::parser::AvauiParser::Parse(source);
    } catch (const avalang::ui::parser::ParseError&) {
        return nullptr;
    }
    if (!parsed.tree) return nullptr;

    auto entry = std::make_unique<CacheEntry>();
    entry->mtime = mtime;
    entry->parsed = std::move(parsed);

    ImportMap ownImportMap = BuildImportMap(entry->parsed.imports);
    avalang::ui::IComponent* root = entry->parsed.tree->Root();
    if (root) {
        ResolveChildrenOf(root, entry->parsed.tree.get(), mergedState, ownImportMap, 0);
    }
    MergeStateMap(mergedState, entry->parsed.state);

    CacheEntry* raw = entry.get();
    cache_[key] = std::move(entry);
    return raw;
}

void UiComponentResolver::ResolveChildrenOf(avalang::ui::IComponent* parent,
                                              avalang::ui::ComponentTree* tree,
                                              std::unordered_map<std::string, std::string>& mergedState,
                                              const ImportMap& importMap, int depth) {
    if (!parent || !tree || depth > kMaxDepth) return;

    auto slotNames = parent->SlotNames();
    for (const auto& slot : slotNames) {
        const auto& children = parent->SlotChildren(slot);
        std::vector<avalang::ui::IComponent*> originals(children.begin(), children.end());
        std::vector<avalang::ui::IComponent*> news;
        news.reserve(originals.size());

        bool changed = false;
        for (avalang::ui::IComponent* child : originals) {
            if (IsComponentCall(child)) {
                std::string typeName = child->TypeName();
                const CacheEntry* comp = LoadComponent(typeName, mergedState, importMap);
                if (comp && comp->parsed.tree) {
                    avalang::ui::IComponent* compRoot = comp->parsed.tree->Root();
                    if (compRoot) {
                        for (const auto& srcChild : compRoot->Children()) {
                            avalang::ui::IComponent* cloned = CloneInto(srcChild, nullptr, tree);
                            if (cloned) news.push_back(cloned);
                        }
                    }
                    changed = true;
                    continue;
                }
            }
            news.push_back(child);
        }

        if (changed) {
            for (avalang::ui::IComponent* c : originals) parent->RemoveChild(c);
            for (avalang::ui::IComponent* c : news) parent->AddChild(c, slot);
        }

        for (avalang::ui::IComponent* c : news) {
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1);
        }
    }
}

avalang::ui::IComponent* UiComponentResolver::CloneInto(const avalang::ui::IComponent* src,
                                                        avalang::ui::IComponent* parent,
                                                        avalang::ui::ComponentTree* dst) {
    if (!src || !dst) return nullptr;
    avalang::ui::IComponent* clone = dst->CreateComponent(src->TypeName());
    if (!clone) return nullptr;

    // IComponent has no PropertyNames enumerator (interface frozen at Fase 13).
    // Copy the allowlist used by LayoutEngine/RenderTree/the VM event bridge --
    // see known_component_properties.h for why this must stay a single list.
    std::size_t count = 0;
    const char* const* names = KnownComponentPropertyNames(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (const auto* p = src->GetProperty(names[i])) {
            clone->SetProperty(names[i], *p);
        }
    }

    if (parent) parent->AddChild(clone);

    for (const auto& child : src->Children()) {
        CloneInto(child, clone, dst);
    }
    return clone;
}

void UiComponentResolver::ResolveImports(avalang::ui::ComponentTree* tree,
                                          const std::vector<std::string>& imports,
                                          std::unordered_map<std::string, std::string>& mergedState) {
    if (!tree) return;
    avalang::ui::IComponent* root = tree->Root();
    if (!root) return;

    ImportMap importMap = BuildImportMap(imports);
    ResolveChildrenOf(root, tree, mergedState, importMap, 0);
}

} // namespace avahost
