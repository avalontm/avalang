#include "design/component_resolver.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "design/avaui_text.h"

namespace fs = std::filesystem;

namespace studio::design {

namespace {

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}


// Recursively assigns a fresh node_uid to `node` and every descendant
// -- same reasoning as MakeNode()/LoadAvauiFile() in design_document.cpp:
// a resolved copy of a cached component tree is a brand-new set of
// canvas nodes, so each instance (even of the same component used
// twice) needs its own independent node_uid for selection/hit-testing
// to work. Mirrors ComponentNode.Clone() + the per-instance uid
// regeneration ComponentResolver.cs relies on implicitly via its own
// node identity model.
void RegenerateUidsRecursive(DesignNode& node) {
    node.node_uid = GenerateNodeUid();
    for (DesignNode& child : node.children) {
        RegenerateUidsRecursive(child);
    }
}

} // namespace

ComponentResolver::ComponentResolver(std::string base_dir) : base_dir_(std::move(base_dir)) {}

bool ComponentResolver::IsComponentCall(const std::string& type) {
    return !type.empty() && std::isupper(static_cast<unsigned char>(type[0]));
}

std::string ComponentResolver::GetComponentName(const std::string& import_path) {
    fs::path p(import_path);
    std::string stem = p.stem().string(); // strips ".avaui"/".ava" if present
    if (stem.empty()) return stem;
    stem[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(stem[0])));
    return stem;
}

std::string ComponentResolver::ResolveImportPath(const std::string& import_path) const {
    std::string path = import_path;
    // Accept the path with or without an explicit extension -- an
    // `import "components/navbar"` line has none (matching the .NET
    // prototype's convention), but nothing stops a hand-written file
    // from spelling it out, so strip a trailing ".avaui" or ".ava"
    // before re-appending ".avaui" below, instead of ending up with
    // "navbar.avaui.avaui".
    for (const char* ext : {".avaui", ".ava"}) {
        size_t ext_len = std::string(ext).size();
        if (path.size() > ext_len && path.compare(path.size() - ext_len, ext_len, ext) == 0) {
            path = path.substr(0, path.size() - ext_len);
            break;
        }
    }

    fs::path candidate = fs::path(base_dir_) / (path + ".avaui");
    std::error_code ec;
    if (fs::exists(candidate, ec) && !ec) {
        return fs::absolute(candidate, ec).string();
    }
    return "";
}

void ComponentResolver::LoadComponent(const std::string& import_path) {
    LoadComponentInternal(import_path);
}

void ComponentResolver::LoadComponentInternal(const std::string& import_path) {
    if (import_cache_.find(import_path) != import_cache_.end()) {
        return; // already loaded (or already attempted) this pass
    }

    std::string resolved_path = ResolveImportPath(import_path);
    if (resolved_path.empty()) {
        return; // no file on disk for this import -- silent, see header comment
    }

    DesignDocument imported_doc;
    std::string load_error;
    if (!LoadAvauiFile(resolved_path, imported_doc, load_error)) {
        return;
    }

    std::string comp_name = GetComponentName(import_path);
    // The imported file's root is always the synthetic "page" wrapper
    // (see design_document.h) -- the actual component tree is its
    // single top-level child. If the file has no children (blank
    // page) or more than one (view has multiple top-level nodes),
    // fall back to using the page node itself as-is; this matches
    // every real component file in the .NET prototype (single root
    // under view) and just degrades gracefully otherwise instead of
    // crashing.
    const DesignNode& component_tree =
        (imported_doc.root.children.size() == 1) ? imported_doc.root.children.front() : imported_doc.root;

    component_cache_[ToLower(comp_name)] = component_tree;
    import_cache_[import_path] = comp_name;

    // Recurse into whatever this component itself imports -- same
    // fixed base_dir_ the whole way down (see the constructor comment
    // on why this is not re-derived from resolved_path's directory).
    for (const std::string& sub_import : imported_doc.imports) {
        if (import_cache_.find(sub_import) == import_cache_.end()) {
            LoadComponentInternal(sub_import);
        }
    }
}

void ComponentResolver::ResolveImports(const DesignDocument& doc) {
    for (const std::string& import_path : doc.imports) {
        LoadComponent(import_path);
    }
}

const DesignNode* ComponentResolver::GetComponentNode(const std::string& name) const {
    auto it = component_cache_.find(ToLower(name));
    if (it == component_cache_.end()) return nullptr;
    return &it->second;
}

DesignNode ComponentResolver::ResolveComponentCall(const DesignNode& node) const {
    std::vector<std::string> expansion_stack;
    return ResolveComponentCallGuarded(node, expansion_stack);
}

DesignNode ComponentResolver::ResolveComponentCallGuarded(const DesignNode& node,
                                                           std::vector<std::string>& expansion_stack) const {
    if (!IsComponentCall(node.type)) {
        return node;
    }

    std::string key = ToLower(node.type);

    // Cycle guard: if this exact component name is already being
    // expanded further up the current call chain (A imports B imports
    // A), stop here and leave the innermost occurrence unresolved --
    // an "empty box" call-site node, same outcome
    // ComponentResolver.cs's DumpNode/ResolveNodeIterative naturally
    // gets from never revisiting a node it already replaced, just made
    // explicit here since our recursion is call-stack based rather
    // than queue-based.
    if (std::find(expansion_stack.begin(), expansion_stack.end(), key) != expansion_stack.end()) {
        return node;
    }

    const DesignNode* component = GetComponentNode(node.type);
    if (component == nullptr) {
        return node; // unresolved reference -- kept as-is, tolerant like the rest of the parser
    }

    DesignNode resolved = *component; // deep copy (DesignNode's members are all value types)
    RegenerateUidsRecursive(resolved);

    // Any explicit props written at the call site (e.g. `Navbar(...)`-
    // style overrides, if a future parser revision supports them)
    // override the component's own defaults for the same key --
    // mirrors ComponentResolver.cs's ResolveComponentCall prop-merge
    // loop. Today's parser never puts props on a bare `Word()` call
    // line (see avaui_text.h), so `node.properties` is normally empty
    // here, but this keeps the resolver correct if that changes.
    for (const PropertyRow& prop : node.properties) {
        auto it = std::find_if(resolved.properties.begin(), resolved.properties.end(),
                                [&](const PropertyRow& existing) { return existing.key == prop.key; });
        if (it != resolved.properties.end()) {
            it->value = prop.value;
        } else {
            resolved.properties.push_back(prop);
        }
    }

    // Recursively resolve component calls that live *inside* the
    // component we just expanded, with this component's name pushed
    // onto the expansion stack so a self-import (direct or nested
    // cycle) is caught.
    expansion_stack.push_back(key);
    ResolveTreeInternal(resolved, expansion_stack);
    expansion_stack.pop_back();

    return resolved;
}

void ComponentResolver::ResolveTree(DesignNode& root) const {
    std::vector<std::string> expansion_stack;
    ResolveTreeInternal(root, expansion_stack);
}

void ComponentResolver::ResolveTreeInternal(DesignNode& node, std::vector<std::string>& expansion_stack) const {
    for (DesignNode& child : node.children) {
        child = ResolveComponentCallGuarded(child, expansion_stack);
        ResolveTreeInternal(child, expansion_stack);
    }
}

} // namespace studio::design
