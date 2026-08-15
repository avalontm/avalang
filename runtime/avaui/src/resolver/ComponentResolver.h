#ifndef AVA_UI_RESOLVER_COMPONENT_RESOLVER_H
#define AVA_UI_RESOLVER_COMPONENT_RESOLVER_H

#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "Fwd.h"
#include "components/ComponentTree.h"
#include "parser/AvauiParser.h"

namespace avalang {
namespace ui {

// Thrown by ComponentResolver when a call site fails validation
// against the callee's declared `params` block (a required param is
// missing, or an argument is passed that was never declared) -- see
// ParamDeclaration's doc comment in AvauiParser.h. Only components
// that declare a `params` block are validated at all; components
// without one keep the old permissive behavior. Derives from
// std::runtime_error so it's caught by the same generic
// `catch (const std::exception&)` handlers that already catch
// parser::ParseError around every render/resolve call site (see
// ui_pipeline_dynamic_renderer.cpp).
class AVA_UI_API ComponentResolveError : public std::runtime_error {
public:
    explicit ComponentResolveError(const std::string& message)
        : std::runtime_error(message) {}
};

class AVA_UI_API ComponentResolver {
public:
    ComponentResolver(std::string projectRoot, std::string componentsDir);
    ComponentResolver(const ComponentResolver&) = delete;
    ComponentResolver& operator=(const ComponentResolver&) = delete;
    ComponentResolver(ComponentResolver&&) = default;
    ComponentResolver& operator=(ComponentResolver&&) = default;

    void ResolveImports(ComponentTree* tree,
                        const std::vector<std::string>& imports,
                        std::unordered_map<std::string, std::string>& mergedState);

    std::vector<IComponent*> ResolveCallSite(IComponent* callSite,
                                             ComponentTree* tree,
                                             const std::vector<std::string>& imports,
                                             std::unordered_map<std::string, std::string>& mergedState);

    static bool IsComponentCall(const IComponent* comp);

private:
    struct CacheEntry {
        std::time_t mtime = 0;
        parser::ParsedAvaui parsed;
    };

    struct ImportMapEntry {
        std::string dottedPath;
        std::string resolvedFilePath;
        // Empty for a plain `import components.confirmdialog`. Set to
        // "dialog" for `import components.confirmdialog as dialog`: this
        // both becomes the callable tag used to invoke the component in
        // the view (`dialog()` instead of `ConfirmDialog()`) and the
        // namespace prefix its own state variables merge into
        // (`dialog.confirmDialogOpen` instead of a bare, page-global
        // `confirmDialogOpen`) -- see MergeStateMap/BuildRenameMap in
        // ComponentResolver.cpp for how each side of that is kept in
        // sync, and ui_vm_state_bridge.cpp/runtime_host.cpp for how a
        // dotted state key like "dialog.confirmDialogOpen" gets bound to
        // a real dict global "dialog" at the AvaLang VM level, so a
        // page's own `code` block can read/write it as
        // `dialog.confirmDialogOpen` (ordinary AvaLang attribute syntax,
        // resolved by the VM's existing generic Dict GETATTR/SETATTR --
        // see OpGetAttr/OpSetAttr in vm_classes.cpp).
        std::string alias;
    };
    using ImportMap = std::unordered_map<std::string, ImportMapEntry>;

    ImportMap BuildImportMap(const std::vector<std::string>& imports) const;

    const CacheEntry* LoadComponent(const std::string& typeName,
                                     std::unordered_map<std::string, std::string>& mergedState,
                                     const ImportMap& importMap);

    void ResolveChildrenOf(IComponent* parent,
                           ComponentTree* tree,
                           std::unordered_map<std::string, std::string>& mergedState,
                           const ImportMap& importMap, int depth);

    std::vector<IComponent*> ResolveOneCallSite(IComponent* callSite,
                                                ComponentTree* tree,
                                                std::unordered_map<std::string, std::string>& mergedState,
                                                const ImportMap& importMap);

    IComponent* CloneInto(const IComponent* src,
                          IComponent* parent,
                          ComponentTree* dst);

    static void ApplyCallSiteOverrides(const IComponent* callSite,
                                        IComponent* clonedRoot);

    // `alias` empty => unchanged behavior (bare keys, first-declared-wins).
    // `alias` non-empty => every key from `addition` is merged in as
    // "alias.key" instead, namespacing this component instance's state
    // so two aliased imports of the same component never collide.
    static void MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                              const std::unordered_map<std::string, std::string>& addition,
                              const std::string& alias = {});

    std::string projectRoot_;
    std::string componentsDir_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    static constexpr int kMaxDepth = 32;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_RESOLVER_COMPONENT_RESOLVER_H
