#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "design/design_document.h"

namespace studio::design {

// Resolves `import "components/x"` declarations and `Componente()`
// calls found inside a DesignDocument's view tree, expanding them into
// the actual subtree of the imported .avaui file.
//
// This is the C++ port of AvaLang.UI/Rendering/ComponentResolver.cs
// (see docs/architecture/08_DESIGNER_VIEW_PLAN.md section 0.1) adapted
// to DesignDocument/DesignNode instead of ComponentNode/ScriptCache --
// there's no AvaVM involved here (the Designer doesn't evaluate
// expressions yet, see plan section 2 point 3), this only walks and
// substitutes tree structure.
//
// One ComponentResolver instance is meant to live for the duration of
// a single resolve pass (e.g. one call from designer_canvas.cpp before
// drawing a frame, or one call from a future "export"/"preview" step)
// -- it caches parsed component files internally so a component
// imported/used multiple times in the same pass is only read and
// parsed from disk once, but nothing here watches the filesystem for
// changes, so a resolver shouldn't be kept around across edits to the
// imported files. Cheap enough to construct fresh each time it's
// needed.
class ComponentResolver {
public:
    // `base_dir` is the PROJECT ROOT that every import path is
    // resolved against -- the same root for the whole recursion, not
    // the directory of whichever file happens to be importing at that
    // moment. This matters for nested imports (a component that itself
    // imports another component): if base_dir were re-derived per
    // file, an import path like "components/navbar" written inside
    // components/navbar.avaui itself (importing a sibling) would
    // resolve relative to components/ instead of the project root,
    // silently breaking. ComponentResolver.cs has the same single
    // fixed _basePath for this exact reason.
    explicit ComponentResolver(std::string base_dir);

    // Loads and parses the .avaui file for `import_path` (e.g.
    // "components/navbar") if it hasn't been loaded yet this pass, and
    // recursively loads whatever that file itself imports. Safe to
    // call more than once with the same path -- a no-op after the
    // first successful load (see import_cache_). Failures (file
    // missing / unreadable) are silent, same forgiving spirit as
    // ParseAvauiText -- a missing component just means
    // ResolveComponentCall later finds nothing and leaves the call
    // node as-is (an empty box in the canvas), not a hard error.
    void LoadComponent(const std::string& import_path);

    // Convenience: loads every path in `doc.imports` (see
    // DesignDocument::imports) via LoadComponent.
    void ResolveImports(const DesignDocument& doc);

    // Turns `import_path` into the component name used as a lookup key
    // (and as the PascalCase type a `Componente()` call site uses):
    // the file's stem, first letter upper-cased. "components/navbar"
    // -> "Navbar". Mirrors ComponentResolver.cs's GetComponentName.
    static std::string GetComponentName(const std::string& import_path);

    // True if `type` looks like a component-call site (PascalCase --
    // same `char.IsUpper(type[0])` check ComponentResolver.cs uses),
    // regardless of whether a matching import was ever loaded. Public
    // so callers like designer_canvas.cpp can tell "this node is a
    // call, just an unresolved one" apart from "this is a real
    // lowercase built-in type" without duplicating the check.
    static bool IsComponentCall(const std::string& type);

    // Looks up an already-loaded component's resolved tree by name
    // (case-insensitive), or nullptr if nothing by that name has been
    // loaded (yet, or ever -- e.g. an import that doesn't exist on
    // disk). The returned pointer is owned by this resolver and only
    // valid for its lifetime.
    const DesignNode* GetComponentNode(const std::string& name) const;

    // Given a call-site node (a leaf DesignNode whose `type` is a
    // PascalCase component call, e.g. type == "Navbar" from a bare
    // `Navbar()` line -- see avaui_text.h), returns a NEW DesignNode
    // that's a full copy of the imported component's tree, with a
    // freshly generated node_uid for every node in the copy (see
    // DesignNode::node_uid -- this keeps canvas selection/hit-testing
    // sound when the same component is used more than once). If `node`
    // isn't a resolvable component call (unknown/lowercase type, or no
    // matching import was ever loaded), returns `node` itself
    // unchanged -- caller can tell nothing happened by comparing
    // node_uid.
    DesignNode ResolveComponentCall(const DesignNode& node) const;

    // Walks `root`'s children recursively and replaces every resolvable
    // component-call node with its resolved subtree in place (see
    // ResolveComponentCall). `root` itself is never replaced, only its
    // descendants -- matching ResolveNode/ResolveNodeIterative in
    // ComponentResolver.cs. A component call inside an imported
    // component's own tree is resolved too (recursive expansion), with
    // a cycle guard: a component already on the current expansion path
    // is left unresolved (kept as the empty call-site box) instead of
    // recursing forever -- see cycle handling notes below.
    void ResolveTree(DesignNode& root) const;

private:
    std::string base_dir_;

    // import_path (as written in an `import "..."` line, e.g.
    // "components/navbar") -> component name ("Navbar"). Prevents
    // reloading/reparsing the same file twice in one pass.
    std::unordered_map<std::string, std::string> import_cache_;

    // component name -> its resolved view-tree root (the single
    // top-level node under the imported file's synthetic `page`, see
    // design_document.h's LoadAvauiFile/ParseAvauiText comment on
    // out_root's shape). Lower-cased key for case-insensitive lookup,
    // same as ComponentResolver.cs's StringComparer.OrdinalIgnoreCase
    // dictionaries.
    std::unordered_map<std::string, DesignNode> component_cache_;

    void LoadComponentInternal(const std::string& import_path);
    void ResolveTreeInternal(DesignNode& node, std::vector<std::string>& expansion_stack) const;
    DesignNode ResolveComponentCallGuarded(const DesignNode& node,
                                           std::vector<std::string>& expansion_stack) const;

    // Resolves `import_path` (with or without a ".avaui" extension) to
    // an absolute file path under base_dir_, or empty string if no
    // candidate exists on disk. Mirrors
    // ComponentResolver.cs::ResolveImportPath, minus the .NET version's
    // extra "sibling of basePath's parent" candidates -- those existed
    // there to compensate for _basePath sometimes being a per-file
    // path; here base_dir_ is always the fixed project root already
    // (see the constructor comment), so a single candidate under it is
    // enough.
    std::string ResolveImportPath(const std::string& import_path) const;
};

} // namespace studio::design
