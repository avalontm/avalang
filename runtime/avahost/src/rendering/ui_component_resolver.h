#pragma once
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/ComponentTree.h"
#include "parser/AvauiParser.h"

namespace avahost {

class UiComponentResolver {
public:
    UiComponentResolver(std::string projectRoot, std::string componentsDir);

    void ResolveImports(avalang::ui::ComponentTree* tree,
                        const std::vector<std::string>& imports,
                        std::unordered_map<std::string, std::string>& mergedState);

private:
    struct CacheEntry {
        std::time_t mtime = 0;
        avalang::ui::parser::ParsedAvaui parsed;
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

    void ResolveChildrenOf(avalang::ui::IComponent* parent,
                           avalang::ui::ComponentTree* tree,
                           std::unordered_map<std::string, std::string>& mergedState,
                           const ImportMap& importMap, int depth);

    static bool IsComponentCall(const avalang::ui::IComponent* comp);

    avalang::ui::IComponent* CloneInto(const avalang::ui::IComponent* src,
                                        avalang::ui::IComponent* parent,
                                        avalang::ui::ComponentTree* dst);

    static void MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                              const std::unordered_map<std::string, std::string>& addition);

    std::string projectRoot_;
    std::string componentsDir_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    static constexpr int kMaxDepth = 32;
};

} // namespace avahost
