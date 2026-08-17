#include "resolver/ComponentResolver.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "components/PropertyValue.h"
#include "parser/AvauiPropertyCoercion.h"
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

bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

void SplitImportAlias(const std::string& raw, std::string& outPath, std::string& outAlias) {
    outPath = raw;
    outAlias.clear();

    std::istringstream iss(raw);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);

    if (tokens.size() >= 3 && tokens[tokens.size() - 2] == "as") {
        outAlias = tokens.back();
        std::ostringstream pathStream;
        for (size_t i = 0; i + 2 < tokens.size(); ++i) {
            if (i) pathStream << ' ';
            pathStream << tokens[i];
        }
        outPath = pathStream.str();
    }
}

std::unordered_map<std::string, std::string> BuildRenameMap(
        const std::unordered_map<std::string, std::string>& ownState,
        const std::string& alias) {
    std::unordered_map<std::string, std::string> renameMap;
    if (alias.empty()) return renameMap;
    for (const auto& [name, value] : ownState) {
        (void)value;
        renameMap[name] = alias + "." + name;
    }
    return renameMap;
}

void RenameStateReferences(IComponent* node,
                            const std::unordered_map<std::string, std::string>& renameMap) {
    if (!node || renameMap.empty()) return;

    for (const auto& propName : node->PropertyNames()) {
        const PropertyValue* prop = node->GetProperty(propName);
        if (!prop || prop->Type() != PropertyType::String) continue;

        const std::string original = prop->AsString();
        std::string rewritten;
        rewritten.reserve(original.size());
        bool changed = false;

        size_t pos = 0;
        while (pos < original.size()) {
            size_t bestMatch = std::string::npos;
            size_t bestLen = 0;
            std::string bestReplacement;
            for (const auto& [from, to] : renameMap) {
                size_t at = original.find(from, pos);
                if (at == std::string::npos) continue;
                if (bestMatch == std::string::npos || at < bestMatch) {
                    bestMatch = at;
                    bestLen = from.size();
                    bestReplacement = to;
                }
            }
            if (bestMatch == std::string::npos) {
                rewritten.append(original, pos, std::string::npos);
                break;
            }

            bool startOk = (bestMatch == 0) || !IsIdentChar(original[bestMatch - 1]);
            size_t afterMatch = bestMatch + bestLen;
            bool endOk = (afterMatch >= original.size()) || !IsIdentChar(original[afterMatch]);

            rewritten.append(original, pos, bestMatch - pos);
            if (startOk && endOk) {
                rewritten += bestReplacement;
                changed = true;
            } else {
                rewritten.append(original, bestMatch, bestLen);
            }
            pos = afterMatch;
        }

        if (changed) {
            node->SetProperty(propName, PropertyValue(rewritten));
        }
    }

    for (IComponent* child : node->Children()) {
        RenameStateReferences(child, renameMap);
    }
}

std::string FormatNumberLiteral(double n) {
    bool negative = std::signbit(n) && n != 0.0;
    double magnitude = std::fabs(n);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(10) << magnitude;
    std::string text = oss.str();

    if (text.find('.') != std::string::npos) {
        size_t lastNonZero = text.find_last_not_of('0');
        if (text[lastNonZero] == '.') --lastNonZero;
        text.erase(lastNonZero + 1);
    }

    return negative ? "-" + text : text;
}

std::string FormatArgLiteral(const PropertyValue& value) {
    switch (value.Type()) {
        case PropertyType::String: {
            std::string escaped;
            escaped.reserve(value.AsString().size());
            for (char c : value.AsString()) {
                if (c == '"' || c == '\\') escaped += '\\';
                escaped += c;
            }
            return "\"" + escaped + "\"";
        }
        case PropertyType::Number:
            return FormatNumberLiteral(value.AsNumber());
        case PropertyType::Bool:
            return value.AsBool() ? "true" : "false";
        default:
            return "nil";
    }
}

PropertyValue ResolveIterableStatic(
        const std::string& iterExpr,
        const std::unordered_map<std::string, std::string>& mergedState) {
    auto it = mergedState.find(iterExpr);
    if (it == mergedState.end()) return PropertyValue();
    return parser::InferValue(it->second);
}

std::unordered_map<std::string, std::string> BuildForItemValueMap(
        const std::string& loopVar, const PropertyRecord& item, int index) {
    std::unordered_map<std::string, std::string> valueMap;
    for (const auto& [fieldName, fieldValue] : item) {
        valueMap[loopVar + "." + fieldName] = FormatArgLiteral(fieldValue);
    }
    valueMap["index"] = FormatNumberLiteral(static_cast<double>(index));
    return valueMap;
}

std::string TrimWs(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool TryDecodeExactFieldRef(const std::string& text, const std::string& loopVar,
                             const PropertyRecord& item, int index, PropertyValue* out) {
    std::string trimmed = TrimWs(text);
    if (trimmed == "index") {
        *out = PropertyValue(static_cast<double>(index));
        return true;
    }
    std::string prefix = loopVar + ".";
    if (trimmed.size() > prefix.size() && trimmed.compare(0, prefix.size(), prefix) == 0) {
        std::string field = trimmed.substr(prefix.size());
        auto it = item.find(field);
        if (it != item.end()) {
            *out = it->second;
            return true;
        }
    }
    return false;
}

void SubstitutePropertiesInPlace(IComponent* node,
                                  const std::unordered_map<std::string, std::string>& valueMap);

void SubstituteForItemInSubtree(IComponent* node, const std::string& loopVar,
                                 const PropertyRecord& item, int index,
                                 const std::unordered_map<std::string, std::string>& itemValueMap) {
    if (!node) return;

    if (ComponentResolver::IsComponentCall(node)) {
        for (const auto& propName : node->PropertyNames()) {
            const PropertyValue* prop = node->GetProperty(propName);
            if (!prop || prop->Type() != PropertyType::String) continue;
            PropertyValue decoded;
            if (TryDecodeExactFieldRef(prop->AsString(), loopVar, item, index, &decoded)) {
                node->SetProperty(propName, decoded);
            }
        }
        return;
    }

    SubstitutePropertiesInPlace(node, itemValueMap);
    for (IComponent* child : node->Children()) {
        SubstituteForItemInSubtree(child, loopVar, item, index, itemValueMap);
    }
}

void ValidateCallSiteArgs(const std::string& typeName,
                           const IComponent* callSite,
                           const std::vector<parser::ParamDeclaration>& declaredParams) {
    if (declaredParams.empty() || !callSite) return;

    std::unordered_map<std::string, const parser::ParamDeclaration*> declared;
    for (const auto& p : declaredParams) declared[p.name] = &p;

    for (const auto& name : callSite->PropertyNames()) {
        if (name == "id" || name == "__unresolvedImportCall") continue;
        if (declared.find(name) == declared.end()) {
            throw ComponentResolveError(
                "unknown argument '" + name + "' passed to " + typeName +
                "(...) -- " + typeName + " does not declare a 'params " + name +
                "' (check for a typo, or add it to " + typeName + "'s params block)");
        }
    }

    for (const auto& p : declaredParams) {
        if (p.hasDefault) continue;
        if (!callSite->GetProperty(p.name)) {
            throw ComponentResolveError(
                "missing required argument '" + p.name + "' in call to " + typeName +
                "(...) -- " + typeName + " declares 'params " + p.name +
                "' with no default, so every call site must pass it");
        }
    }
}

std::unordered_map<std::string, std::string> BuildArgValueMap(
        const IComponent* callSite,
        const std::unordered_map<std::string, std::string>& ownState,
        const std::vector<parser::ParamDeclaration>& declaredParams) {
    std::unordered_map<std::string, std::string> map;
    if (!callSite) return map;
    for (const auto& name : callSite->PropertyNames()) {
        if (name == "id" || name == "__unresolvedImportCall") continue;
        if (ownState.find(name) != ownState.end()) continue;
        if (const auto* prop = callSite->GetProperty(name)) {
            map[name] = FormatArgLiteral(*prop);
        }
    }
    for (const auto& p : declaredParams) {
        if (!p.hasDefault) continue;
        if (map.find(p.name) != map.end()) continue;
        if (ownState.find(p.name) != ownState.end()) continue;
        map[p.name] = FormatArgLiteral(p.defaultValue);
    }
    return map;
}

void SubstitutePropertiesInPlace(IComponent* node,
                                  const std::unordered_map<std::string, std::string>& valueMap) {
    if (!node || valueMap.empty()) return;

    for (const auto& propName : node->PropertyNames()) {
        const PropertyValue* prop = node->GetProperty(propName);
        if (!prop || prop->Type() != PropertyType::String) continue;

        const std::string original = prop->AsString();
        std::string rewritten;
        rewritten.reserve(original.size());
        bool changed = false;

        size_t pos = 0;
        while (pos < original.size()) {
            size_t bestMatch = std::string::npos;
            size_t bestLen = 0;
            std::string bestReplacement;
            for (const auto& [from, to] : valueMap) {
                size_t at = original.find(from, pos);
                if (at == std::string::npos) continue;
                if (bestMatch == std::string::npos || at < bestMatch) {
                    bestMatch = at;
                    bestLen = from.size();
                    bestReplacement = to;
                }
            }
            if (bestMatch == std::string::npos) {
                rewritten.append(original, pos, std::string::npos);
                break;
            }

            bool startOk = (bestMatch == 0) || !IsIdentChar(original[bestMatch - 1]);
            size_t afterMatch = bestMatch + bestLen;
            bool endOk = (afterMatch >= original.size()) || !IsIdentChar(original[afterMatch]);

            rewritten.append(original, pos, bestMatch - pos);
            if (startOk && endOk) {
                rewritten += bestReplacement;
                changed = true;
            } else {
                rewritten.append(original, bestMatch, bestLen);
            }
            pos = afterMatch;
        }

        if (changed) {
            node->SetProperty(propName, PropertyValue(rewritten));
        }
    }
}

void SubstituteCallArgs(IComponent* node,
                         const std::unordered_map<std::string, std::string>& valueMap) {
    if (!node || valueMap.empty()) return;
    SubstitutePropertiesInPlace(node, valueMap);
    for (IComponent* child : node->Children()) {
        SubstituteCallArgs(child, valueMap);
    }
}

void CheckNoHardcodedIdOnReusableComponent(const std::string& typeName,
                                            const std::vector<parser::ParamDeclaration>& declaredParams,
                                            const IComponent* node) {
    if (declaredParams.empty() || !node) return;

    if (const auto* idProp = node->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
            throw ComponentResolveError(
                typeName + " declares params -- so it's meant to be instantiated more than "
                "once -- but hardcodes id = \"" + idProp->AsString() + "\" on one of its own "
                "elements (" + node->TypeName() + "). Every instance of " + typeName +
                " would share that exact id, which breaks component refs the moment " +
                typeName + " is used more than once on the same page. Remove the id -- most "
                "elements don't need one -- or, if you genuinely need a per-instance ref, "
                "give the id a value derived from one of " + typeName + "'s own params instead "
                "of a fixed literal.");
        }
    }

    for (const IComponent* child : node->Children()) {
        CheckNoHardcodedIdOnReusableComponent(typeName, declaredParams, child);
    }
}

} // namespace

ComponentResolver::ComponentResolver(std::string projectRoot, std::string componentsDir)
    : projectRoot_(std::move(projectRoot)), componentsDir_(std::move(componentsDir)) {}

bool ComponentResolver::IsComponentCall(const IComponent* comp) {
    if (!comp) return false;
    if (comp->TypeName() == "Slot") return false;
    if (const auto* prop = comp->GetProperty("__unresolvedImportCall")) {
        if (prop->Type() == PropertyType::Bool && prop->AsBool()) {
            return true;
        }
    }
    return false;
}

void ComponentResolver::MergeStateMap(std::unordered_map<std::string, std::string>& merged,
                                       const std::unordered_map<std::string, std::string>& addition,
                                       const std::string& alias) {
    for (const auto& [k, v] : addition) {
        std::string key = alias.empty() ? k : (alias + "." + k);
        if (merged.find(key) == merged.end()) {
            merged[key] = v;
        }
    }
}

ComponentResolver::ImportMap ComponentResolver::BuildImportMap(const std::vector<std::string>& imports) const {
    ImportMap map;
    for (const auto& raw : imports) {
        std::string dotted;
        std::string alias;
        SplitImportAlias(raw, dotted, alias);

        std::string tag = alias.empty() ? CallableTagFromDotted(dotted) : alias;
        if (tag.empty()) continue;
        fs::path resolved = ResolveDottedAvauiPath(projectRoot_, dotted);
        map[tag] = ImportMapEntry{dotted, resolved.string(), alias};
    }
    return map;
}

const ComponentResolver::CacheEntry* ComponentResolver::LoadComponent(
        const std::string& typeName,
        std::unordered_map<std::string, std::string>& mergedState,
        const ImportMap& importMap) {
    fs::path filePath;
    std::string alias;
    auto it = importMap.find(typeName);
    if (it != importMap.end()) {
        filePath = it->second.resolvedFilePath;
        alias = it->second.alias;
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
        MergeStateMap(mergedState, cacheIt->second->parsed.state, alias);
        return cacheIt->second.get();
    }

    std::string source;
    if (!ReadFile(key, source)) return nullptr;

    parser::ParsedAvaui parsed = parser::AvauiParser::Parse(source, key);
    if (!parsed.tree) return nullptr;

    auto entry = std::make_unique<CacheEntry>();
    entry->mtime = mtime;
    entry->parsed = std::move(parsed);

    ImportMap ownImportMap = BuildImportMap(entry->parsed.imports);
    IComponent* root = entry->parsed.tree->Root();
    if (root) {
        CheckNoHardcodedIdOnReusableComponent(typeName, entry->parsed.params, root);
        ResolveChildrenOf(root, entry->parsed.tree.get(), mergedState, ownImportMap, 0);
    }
    MergeStateMap(mergedState, entry->parsed.state, alias);

    CacheEntry* raw = entry.get();
    cache_[key] = std::move(entry);
    return raw;
}

void ComponentResolver::ResolveChildrenOf(IComponent* parent,
                                           ComponentTree* tree,
                                           std::unordered_map<std::string, std::string>& mergedState,
                                           const ImportMap& importMap, int depth,
                                           bool expandLoops) {
    if (!parent || !tree || depth > kMaxDepth) return;
    if (parent->TypeName() == "For") {
        if (!expandLoops) {
            // Runtime pipelines own For's per-render expansion (VM-driven).
            // Leave the template untouched here so it isn't consumed
            // against the file's static initial state.
            return;
        }
        std::vector<IComponent*> produced = ExpandForNode(parent, tree, mergedState, importMap);
        for (IComponent* c : produced) {
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1, expandLoops);
        }
        return;
    }
    if (parent->TypeName() == "ListView") {
        if (!expandLoops) {
            // Same reasoning as "For": the live runtime pipeline expands
            // ListView itself using the current VM state per render.
            return;
        }
        std::vector<IComponent*> produced = ExpandListViewNode(parent, tree, mergedState, importMap);
        for (IComponent* c : produced) {
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1, expandLoops);
        }
        return;
    }

    auto slotNames = parent->SlotNames();
    for (const auto& slot : slotNames) {
        const auto& children = parent->SlotChildren(slot);
        std::vector<IComponent*> originals(children.begin(), children.end());
        std::vector<IComponent*> news;
        news.reserve(originals.size());

        bool changed = false;
        for (IComponent* child : originals) {
            if (IsComponentCall(child)) {
                std::vector<IComponent*> resolved = ResolveOneCallSite(child, tree, mergedState, importMap);
                for (IComponent* c : resolved) news.push_back(c);
                changed = true;
                continue;
            }
            news.push_back(child);
        }

        if (changed) {
            for (IComponent* c : originals) parent->RemoveChild(c);
            for (IComponent* c : news) parent->AddChild(c, slot);
        }

        for (IComponent* c : news) {
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1, expandLoops);
        }
    }
}

std::vector<IComponent*> ComponentResolver::ExpandForNode(
        IComponent* forNode, ComponentTree* tree,
        std::unordered_map<std::string, std::string>& mergedState,
        const ImportMap& importMap) {
    std::vector<IComponent*> produced;
    if (!forNode || !tree) return produced;

    const auto* loopVarProp = forNode->GetProperty("loopVar");
    const auto* iterProp = forNode->GetProperty("iterable");
    if (!loopVarProp || !iterProp) return produced;
    if (loopVarProp->Type() != PropertyType::String || iterProp->Type() != PropertyType::String) {
        return produced;
    }

    std::string loopVar = loopVarProp->AsString();
    std::string iterExpr = iterProp->AsString();

    PropertyValue iterable = ResolveIterableStatic(iterExpr, mergedState);
    if (iterable.Type() != PropertyType::List) return produced;

    std::vector<IComponent*> templateChildren = forNode->Children();
    if (templateChildren.empty()) return produced;

    for (IComponent* templateChild : templateChildren) {
        forNode->RemoveChild(templateChild);
    }

    const PropertyList& items = iterable.AsList();
    int index = 0;
    for (const PropertyRecord& item : items) {
        std::unordered_map<std::string, std::string> itemValueMap =
            BuildForItemValueMap(loopVar, item, index);

        for (IComponent* templateChild : templateChildren) {
            IComponent* clone = CloneInto(templateChild, forNode, tree);
            if (!clone) continue;
            SubstituteForItemInSubtree(clone, loopVar, item, index, itemValueMap);

            if (IsComponentCall(clone)) {
                std::vector<IComponent*> resolved =
                    ResolveOneCallSite(clone, tree, mergedState, importMap);
                forNode->RemoveChild(clone);
                for (IComponent* r : resolved) {
                    forNode->AddChild(r);
                    produced.push_back(r);
                }
            } else {
                produced.push_back(clone);
            }
        }
        ++index;
    }

    return produced;
}

std::vector<IComponent*> ComponentResolver::ExpandListViewNode(
        IComponent* listViewNode, ComponentTree* tree,
        std::unordered_map<std::string, std::string>& mergedState,
        const ImportMap& importMap) {
    std::vector<IComponent*> produced;
    if (!listViewNode || !tree) return produced;

    const auto* sourceProp = listViewNode->GetProperty("source");
    if (!sourceProp || sourceProp->Type() != PropertyType::String) return produced;

    std::string loopVar = "item";
    if (const auto* asProp = listViewNode->GetProperty("as")) {
        if (asProp->Type() == PropertyType::String && !asProp->AsString().empty()) {
            loopVar = asProp->AsString();
        }
    }

    // ListView doesn't need an explicit `for` child: its own children ARE
    // the item template. Relabel to the properties ExpandForNode already
    // knows how to consume and delegate, so both engines share one
    // battle-tested expansion path instead of duplicating loop logic.
    listViewNode->SetProperty("loopVar", PropertyValue(loopVar));
    listViewNode->SetProperty("iterable", PropertyValue(sourceProp->AsString()));
    return ExpandForNode(listViewNode, tree, mergedState, importMap);
}

std::vector<IComponent*> ComponentResolver::ResolveOneCallSite(
        IComponent* callSite, ComponentTree* tree,
        std::unordered_map<std::string, std::string>& mergedState,
        const ImportMap& importMap) {
    std::vector<IComponent*> resolved;
    if (!callSite || !tree) return resolved;

    std::string typeName = callSite->TypeName();
    const CacheEntry* comp = LoadComponent(typeName, mergedState, importMap);
    if (!comp || !comp->parsed.tree) return resolved;

    ValidateCallSiteArgs(typeName, callSite, comp->parsed.params);

    std::string alias;
    auto importIt = importMap.find(typeName);
    if (importIt != importMap.end()) alias = importIt->second.alias;
    std::unordered_map<std::string, std::string> renameMap =
        BuildRenameMap(comp->parsed.state, alias);

    IComponent* compRoot = comp->parsed.tree->Root();
    if (!compRoot) return resolved;

    for (const auto& srcChild : compRoot->Children()) {
        IComponent* cloned = CloneInto(srcChild, nullptr, tree);
        if (!cloned) continue;
        RenameStateReferences(cloned, renameMap);
        ApplyCallSiteOverrides(callSite, cloned);
        SubstituteCallArgs(cloned, BuildArgValueMap(callSite, comp->parsed.state, comp->parsed.params));
        resolved.push_back(cloned);
    }
    return resolved;
}

std::vector<IComponent*> ComponentResolver::ResolveCallSite(
        IComponent* callSite, ComponentTree* tree, const std::vector<std::string>& imports,
        std::unordered_map<std::string, std::string>& mergedState) {
    if (!callSite || !tree || !IsComponentCall(callSite)) return {};

    ImportMap importMap = BuildImportMap(imports);
    std::vector<IComponent*> resolved = ResolveOneCallSite(callSite, tree, mergedState, importMap);
    for (IComponent* c : resolved) {
        ResolveChildrenOf(c, tree, mergedState, importMap, 0);
    }
    return resolved;
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
                                          std::unordered_map<std::string, std::string>& mergedState,
                                          bool expandLoops) {
    if (!tree) return;
    IComponent* root = tree->Root();
    if (!root) return;

    ImportMap importMap = BuildImportMap(imports);
    ResolveChildrenOf(root, tree, mergedState, importMap, 0, expandLoops);
}

} // namespace ui
} // namespace avalang