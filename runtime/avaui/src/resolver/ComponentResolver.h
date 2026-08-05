#ifndef AVA_UI_RESOLVER_COMPONENT_RESOLVER_H
#define AVA_UI_RESOLVER_COMPONENT_RESOLVER_H

#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Export.h"
#include "Fwd.h"
#include "components/ComponentTree.h"
#include "parser/AvauiParser.h"

namespace avalang {
namespace ui {

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

    static bool IsComponentCall(const IComponent* comp);

private:
    struct CacheEntry {
        std::time_t mtime = 0;
        parser::ParsedAvaui parsed;
    };

    struct ImportMapEntry {
        std::string dottedPath;
        std::string resolvedFilePath;
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

    IComponent* CloneInto(const IComponent* src,
                          IComponent* parent,
                          ComponentTree* dst);

    static void ApplyCallSiteOverrides(const IComponent* callSite,
                                        IComponent* clonedRoot);

    static void MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                              const std::unordered_map<std::string, std::string>& addition);

    std::string projectRoot_;
    std::string componentsDir_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    static constexpr int kMaxDepth = 32;
};

} // namespace ui
} // namespace avalang

#endif // AVA_UI_RESOLVER_COMPONENT_RESOLVER_H
