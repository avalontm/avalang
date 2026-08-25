#include "design/component_resolver.h"

#include <unordered_map>

#include "resolver/ComponentResolver.h"

namespace studio::design {

void ResolveImportsForDocument(DesignDocument& doc, const std::string& projectRoot) {
    if (!doc.tree || doc.imports.empty()) return;
    std::unordered_map<std::string, std::string> mergedState;
    for (const auto& row : doc.initial_state) {
        mergedState[row.key] = row.value;
    }
    avalang::ui::ComponentResolver resolver(projectRoot, projectRoot + "/components");
    resolver.ResolveImports(doc.tree.get(), doc.imports, mergedState);
    doc.initial_state.clear();
    for (const auto& [k, v] : mergedState) {
        doc.initial_state.push_back(PropertyRow{k, v});
    }
}

}
