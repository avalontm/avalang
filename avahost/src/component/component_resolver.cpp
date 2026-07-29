#include "component/component_resolver.h"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "component/dotted_path.h"

namespace fs = std::filesystem;
using nlohmann::json;

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

// AvaUiDocument::importsJson -- a JSON array of dotted import strings
// ("components.navbar", ...), see core/src/ui/avaui_text.cpp's
// ParseImportLines / ImportsToJson. Malformed/absent JSON just yields
// no imports rather than failing the page.
std::vector<std::string> ParseImportsJson(const std::string& importsJson) {
    std::vector<std::string> out;
    try {
        json arr = json::parse(importsJson.empty() ? "[]" : importsJson);
        if (arr.is_array()) {
            for (const auto& v : arr) {
                if (v.is_string()) out.push_back(v.get<std::string>());
            }
        }
    } catch (const json::exception&) {
        // fall through, return what we have (empty)
    }
    return out;
}

} // namespace

ComponentResolver::ComponentResolver(RuntimeHost& runtime, std::string projectRoot, std::string componentsDir)
    : runtime_(runtime), projectRoot_(std::move(projectRoot)), componentsDir_(std::move(componentsDir)) {}

bool ComponentResolver::IsComponentCall(AvaComponent* comp) {
    if (!comp) return false;

    const char* type = ava_ui_get_component_type(comp);
    if (!type || !*type || !std::isupper(static_cast<unsigned char>(type[0]))) return false;

    const char* id = ava_ui_get_id(comp);
    if (id && *id) return false;

    if (ava_ui_property_count(comp) != 0) return false;
    if (ava_ui_event_count(comp) != 0) return false;
    if (ava_ui_child_count(comp) != 0) return false;

    return true;
}

void ComponentResolver::MergeStateJson(std::string& mergedStateJson, const std::string& componentStateJson) {
    json merged;
    json addition;
    try {
        merged = json::parse(mergedStateJson.empty() ? "{}" : mergedStateJson);
        addition = json::parse(componentStateJson.empty() ? "{}" : componentStateJson);
    } catch (const json::exception&) {
        return; // malformed state text -- fail soft, keep mergedStateJson unchanged
    }
    if (!merged.is_object()) merged = json::object();
    if (!addition.is_object()) return;

    // First writer wins: page state (already in `merged`) beats every
    // imported component's, and among components, whichever was
    // resolved first keeps its value -- see ResolveImports's header
    // comment.
    for (auto it = addition.begin(); it != addition.end(); ++it) {
        if (!merged.contains(it.key())) merged[it.key()] = it.value();
    }
    mergedStateJson = merged.dump();
}

ComponentResolver::ImportMap ComponentResolver::BuildImportMap(const std::string& importsJson) const {
    ImportMap map;
    for (const auto& dotted : ParseImportsJson(importsJson)) {
        std::string tag = CallableTagFromDotted(dotted);
        if (!tag.empty()) map[tag] = dotted; // last import for a given tag wins
    }
    return map;
}

RuntimeHost::AvaUiDocument* ComponentResolver::LoadComponent(const std::string& typeName,
                                                               std::string& mergedStateJson,
                                                               const ImportMap& importMap) {
    fs::path filePath;
    auto importIt = importMap.find(typeName);
    if (importIt != importMap.end()) {
        // Explicit `import ...` line for this tag -- resolve wherever
        // it actually points (component/dotted_path.h), not
        // necessarily under componentsDir_.
        filePath = ResolveDottedAvauiPath(projectRoot_, importIt->second);
    } else {
        // Legacy fallback: no import line declared this tag -- keep
        // the original convention so existing pages that never
        // adopted `import` keep working.
        filePath = fs::path(componentsDir_) / (typeName + ".avaui");
    }

    std::error_code ec;
    auto writeTime = fs::last_write_time(filePath, ec);
    if (ec) return nullptr; // file doesn't exist

    // Portable fs::file_time_type -> time_t conversion (avoids
    // std::chrono::clock_cast, whose file_clock support varies by
    // standard library version) -- same trick used elsewhere in
    // codebases needing this: rebase the file time onto system_clock's
    // epoch via "now" in both clocks.
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t mtime = std::chrono::system_clock::to_time_t(sctp);

    std::string key = filePath.string();
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second->mtime == mtime) {
        // Cache hit: this file's own imports were already resolved
        // (recursively) the first time it was loaded below, so no
        // further ResolveChildrenOf call is needed here -- just merge
        // its (already-resolved-at-cache-time) state in.
        return &it->second->doc;
    }

    std::string source;
    if (!ReadFile(filePath.string(), source)) return nullptr;

    RuntimeHost::AvaUiDocument doc = runtime_.ParseAvaUiFile(source);
    if (!doc.ok || !doc.tree) return nullptr;

    auto entry = std::make_unique<CacheEntry>();
    entry->mtime = mtime;
    entry->doc = std::move(doc);

    // Resolve this component's OWN imports (e.g. a Navbar that itself
    // uses a Button()) once, right here, before caching -- against
    // THIS FILE's own import lines, not the caller's -- so every later
    // splice of this cached tree is already fully expanded and doesn't
    // need to re-walk on every request that uses it.
    ImportMap ownImportMap = BuildImportMap(entry->doc.importsJson);
    AvaComponent* root = ava_ui_get_root(entry->doc.tree);
    if (root) ResolveChildrenOf(root, entry->doc.stateJson, 0, ownImportMap);

    CacheEntry* raw = entry.get();
    cache_[key] = std::move(entry);
    return &raw->doc;
}

void ComponentResolver::ResolveChildrenOf(AvaComponent* parent, std::string& mergedStateJson, int depth,
                                           const ImportMap& importMap) {
    if (!parent || depth > kMaxDepth) return;

    size_t count = ava_ui_child_count(parent);
    std::vector<AvaComponent*> originalChildren;
    originalChildren.reserve(count);
    for (size_t i = 0; i < count; ++i) originalChildren.push_back(ava_ui_get_child(parent, i));

    std::vector<AvaComponent*> newChildren;
    newChildren.reserve(count);
    bool changed = false;

    for (AvaComponent* child : originalChildren) {
        if (IsComponentCall(child)) {
            // Copy before any further ava_ui_* call -- ava_ui_get_
            // component_type's return value isn't guaranteed to
            // outlive the next one (see avalang.h's note on
            // ava_ui_get_id/property_key_at).
            std::string typeName = ava_ui_get_component_type(child);
            RuntimeHost::AvaUiDocument* compDoc = LoadComponent(typeName, mergedStateJson, importMap);
            if (compDoc && compDoc->tree) {
                AvaComponent* compRoot = ava_ui_get_root(compDoc->tree);
                size_t rc = compRoot ? ava_ui_child_count(compRoot) : 0;
                for (size_t j = 0; j < rc; ++j) {
                    newChildren.push_back(ava_ui_get_child(compRoot, j));
                }
                MergeStateJson(mergedStateJson, compDoc->stateJson);
                changed = true;
                continue; // the call node itself is dropped, replaced by its component's children
            }
            // File missing/unparseable -- fall through and keep the
            // call node as-is (renders as an empty <div>, same
            // behavior as before this resolver existed, rather than
            // failing the whole page over one bad import).
        }
        newChildren.push_back(child);
    }

    if (changed) {
        for (AvaComponent* c : originalChildren) ava_ui_remove_child(parent, c);
        for (AvaComponent* c : newChildren) ava_ui_add_child(parent, c);
    }

    // Recurse so nested containers (column/row/... holding further
    // Type() calls several levels deep) get resolved too. Safe to
    // recurse into every surviving child, spliced-in or original alike
    // -- a spliced-in node's own imports were already resolved when
    // its owning component file was first cached (see LoadComponent),
    // so this is a cheap re-walk of an already-import-free subtree in
    // that case, and the real (possibly first) resolution pass for any
    // untouched sibling subtree.
    for (AvaComponent* c : newChildren) {
        ResolveChildrenOf(c, mergedStateJson, depth + 1, importMap);
    }
}

void ComponentResolver::ResolveImports(AvaComponentTree* pageTree, std::string& mergedStateJson,
                                        const std::vector<std::string>& imports) {
    if (!pageTree) return;
    AvaComponent* root = ava_ui_get_root(pageTree);
    if (!root) return;

    ImportMap importMap;
    for (const auto& dotted : imports) {
        std::string tag = CallableTagFromDotted(dotted);
        if (!tag.empty()) importMap[tag] = dotted;
    }

    ResolveChildrenOf(root, mergedStateJson, 0, importMap);
}

} // namespace avahost
