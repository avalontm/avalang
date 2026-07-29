#pragma once
// AvaHost.Component -- Fase 2 module 1
// (docs/architecture/AVAHOST_PROGRESS.md row 8: "Import de componentes").
//
// A .avaui page's `view` block can reference an imported component by
// its PascalCase name as a bare call, e.g. `Navbar()` after
// `import components.navbar`. core/src/ui/avaui_text.cpp's parser
// already recognizes that call form (see WriteNode's `is_call_form`
// there) but leaves it as an empty AvaComponent node -- avahost/src/
// rendering/html_renderer.cpp then has nothing to recurse into, so it
// renders as an empty <div></div>. ComponentResolver is the missing
// step: it walks a parsed page tree and splices in each referenced
// component's own (already-parsed) tree in place.
//
// Resolution order for a call node's type name (e.g. "Navbar"):
//   1. An `import ...` line in the SAME FILE whose dotted path's last
//      segment capitalizes to this tag (component/dotted_path.h's
//      CallableTagFromDotted) -- e.g. `import components.navbar` ->
//      `Navbar()` resolves to <projectRoot>/components/navbar.avaui,
//      wherever that file actually lives.
//   2. Fallback, for files with no matching import line: the legacy
//      convention, <componentsDir>/<Type>.avaui (HostOptions::
//      componentsDir) -- kept so existing pages that call components
//      by name only, without declaring an import, keep working.
//
// Operates directly on AvaComponent*/AvaComponentTree* via avalang.h's
// ava_ui_* C API -- same precedent as avahost/src/rendering/
// html_renderer.h, which already includes avalang.h for the same
// reason (walking/reading the UI tree isn't VM script execution, so it
// doesn't need to go through RuntimeHost the way ava_compile/ava_run
// do -- see runtime/runtime_host.h's header comment).
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "avalang.h"
#include "runtime/runtime_host.h"

namespace avahost {

class ComponentResolver {
public:
    // `runtime` parses each .avaui file this resolver loads
    // (RuntimeHost owns the one VM allowed to call
    // ava_ui_parse_avaui_text). `projectRoot` is the project's root
    // folder, used to resolve any dotted `import` line (see class
    // comment above). `componentsDir` stays as the legacy fallback
    // for call nodes with no matching import line.
    ComponentResolver(RuntimeHost& runtime, std::string projectRoot, std::string componentsDir);

    // Walks `pageTree` in place, replacing every "component call" node
    // (PascalCase type, no id/properties/events/children -- the exact
    // same shape core/src/ui/avaui_text.cpp's WriteNode checks via
    // `is_call_form`) with the (recursively resolved) children of the
    // imported file's own view (resolution order: see class comment
    // above). Safe to call on a tree with no such nodes at all (common
    // case, most pages import nothing) -- it's then a no-op walk.
    //
    // `imports` is the page's OWN dotted `import` lines (its
    // AvaUiDocument::importsJson, already parsed to a vector) -- used
    // to build this file's tag->path lookup. Each imported component's
    // own imports are read from its own file when it's loaded, so
    // nested imports (a Navbar that itself imports a Button) resolve
    // against the Navbar's imports, not the page's.
    //
    // `mergedStateJson` starts as the page's own `state` block (shape:
    // AvaUiDocument::stateJson) and accumulates every resolved
    // component's own `state` block into it -- key collisions keep
    // whatever was already present (page state, or whichever component
    // was spliced in first) rather than being overwritten, so a
    // page-level `state` always wins over an imported component's.
    void ResolveImports(AvaComponentTree* pageTree, std::string& mergedStateJson,
                         const std::vector<std::string>& imports);

private:
    RuntimeHost& runtime_;
    std::string projectRoot_;
    std::string componentsDir_;

    // Guards against a cyclic import (A imports B imports A) walking
    // forever -- 32 levels is far deeper than any real component tree,
    // so hitting it always means a cycle, not a legitimately deep app.
    static constexpr int kMaxDepth = 32;

    // One imported file's parsed-and-resolved document, cached by
    // resolved file path so `Navbar()` used many times across many
    // requests only ever costs one parse+resolve, not one per use
    // (plan Fase 2 decision: "cachea componentes, no re-parsea").
    // Invalidated by mtime so `--watch` hot reload still picks up an
    // edited component file -- see LoadComponent.
    struct CacheEntry {
        std::time_t mtime = 0;
        RuntimeHost::AvaUiDocument doc;
    };
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;

    // Tag ("Navbar") -> resolved file path, built once per ResolveImports/
    // recursive-load call from that file's own import lines (see
    // BuildImportMap in the .cpp).
    using ImportMap = std::unordered_map<std::string, std::string>;

    // Loads (or returns the cached, still-fresh) parsed document for
    // `typeName`, resolved via `importMap` first, falling back to
    // `<componentsDir_>/<typeName>.avaui` when `typeName` has no entry
    // there (see class comment's resolution order). Returns nullptr
    // when the file doesn't exist or fails to parse -- ResolveNode
    // then leaves the call node as-is (renders empty, same as today)
    // rather than failing the whole page over one bad import.
    RuntimeHost::AvaUiDocument* LoadComponent(const std::string& typeName, std::string& mergedStateJson,
                                               const ImportMap& importMap);

    // Recursive worker: resolves every component-call child of
    // `parent`, splicing in each one's own (already fully-resolved,
    // see LoadComponent) children, then recurses into whatever's left
    // so a spliced-in component's own descendants (which may
    // themselves import further components -- LoadComponent already
    // handled that when it first cached them) and any untouched
    // sibling subtrees are both walked too.
    void ResolveChildrenOf(AvaComponent* parent, std::string& mergedStateJson, int depth,
                            const ImportMap& importMap);

    // True when `comp` is exactly the "bare Type() call" shape
    // is_call_form checks in core/src/ui/avaui_text.cpp's WriteNode:
    // PascalCase type, no id, no properties, no events, no children.
    static bool IsComponentCall(AvaComponent* comp);

    // Adds every key in `componentStateJson` not already present in
    // `mergedStateJson` into it (first writer wins) -- see
    // ResolveImports's header comment on merge precedence.
    static void MergeStateJson(std::string& mergedStateJson, const std::string& componentStateJson);

    // Parses an AvaUiDocument::importsJson string (a JSON array of
    // dotted import strings) into a tag->path ImportMap via
    // component/dotted_path.h's CallableTagFromDotted/
    // ResolveDottedAvauiPath.
    ImportMap BuildImportMap(const std::string& importsJson) const;
};

} // namespace avahost
