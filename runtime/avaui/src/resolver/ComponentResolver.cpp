#include "resolver/ComponentResolver.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "components/PropertyValue.h"
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

// Splits a raw `.avaui` import spec ("components.confirmdialog" or
// "components.confirmdialog as dialog") into its dotted module path and
// optional alias. AvauiParser::Parse intentionally stores whatever
// follows `import ` verbatim (see AvauiParser.cpp -- it never looks for
// `as` itself), so this is the one place that actually interprets the
// suffix. Keeping the split here -- rather than in the parser or in
// ParsedAvaui::imports's type -- means every other reader of that vector
// (AvauiWriter's round-trip, Ava Studio's DesignDocument, the public C
// API's JSON export) keeps treating these as opaque strings, unchanged.
void SplitImportAlias(const std::string& raw, std::string& outPath, std::string& outAlias) {
    outPath = raw;
    outAlias.clear();

    std::istringstream iss(raw);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);

    // A standalone `as` token, second-to-last, with something after it:
    // `components.confirmdialog as dialog`. Anything else (no `as`, or
    // `as` not in that position) is treated as a plain, alias-less path,
    // so a module segment that merely contains "as" never misfires.
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

// Builds a bare-state-name -> "alias.name" rename table for one imported
// component instance. Empty alias => empty map, i.e. "nothing to
// rewrite" (the plain, unnamespaced import stays exactly as it works
// today).
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

// Rewrites bare references to an imported component's own state (e.g.
// `confirmDialogTitle`) into their namespaced form (`dialog.
// confirmDialogTitle`) across every String-typed property of `node` and
// its whole subtree -- this is what makes the component's own view
// bindings (`title = confirmDialogTitle`, `isOpen = confirmDialogOpen`)
// keep working once its state has been merged into the page under a
// prefix instead of as bare globals. Only whole-identifier matches are
// replaced (checked via IsIdentChar on both sides of every match), so
// this can never clobber an unrelated identifier that happens to contain
// one of these names as a substring. Non-String properties (numbers,
// bools) are left alone, and any property whose text doesn't reference
// one of THIS component's own state names is untouched byte-for-byte --
// in particular `click = ConfirmDialogAccept()` handler bindings, since
// handler names are never part of `ownState` (they stay page-level,
// unprefixed, exactly per ConfirmDialog.avaui's own documented
// convention).
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

// AvaLang's NUMBER token (grammar: `DIGIT+ ('.' DIGIT+)?`, see
// AvaLang.g4) has no exponent notation and no sign of its own --
// negatives are a unary '-' applied to a NUMBER atom, a separate
// grammar rule. std::ostringstream's default operator<<(double) uses
// %g-style formatting, which switches to scientific notation
// ("1.23457e+06") past ~6 significant digits and would hand back a
// literal AvaLang's lexer can't parse at all -- silently breaking any
// call-site argument once an id/price/quantity crosses that threshold,
// or once a price needs more than 6 significant digits of precision
// (already possible with ordinary currency amounts). Format explicitly
// as fixed-point instead, then trim the padding.
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

// Formats a call-site argument's PropertyValue as literal AvaLang
// source text, so SubstituteCallArgs can inline it wherever the
// component references that argument by (bare) name -- e.g.
// price = "3.80" becomes the literal `"3.80"`, productId = 3 becomes
// the literal `3`. String values are re-quoted/escaped since
// AsString() holds the already-unquoted text (InferValue stripped the
// quotes when the call-site argument was first parsed).
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

// Validates a call site (`ProductCard(name = "Latte", productId = 3)`,
// i.e. `callSite`) against the callee's own declared `params` block
// (`declaredParams`, from ProductCard.avaui). Only runs at all when
// the callee declares at least one param -- an empty `declaredParams`
// means "no params block in this file", which keeps every existing
// component that predates this feature working exactly as before,
// unvalidated. `typeName` is only used to make the thrown message
// identify which component/call site is at fault.
//
// Two failure modes, both reported as ComponentResolveError so they
// surface at resolve time (page render), not silently at click time
// as an "undefined variable" deep in a handler:
//   - a required (no-default) param the call site never passed
//   - an argument the call site passed that isn't declared at all
//     (almost always a typo -- e.g. `ProductCrad(prodctId = 3)`)
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

// Builds a bare-name -> literal-source-text map from a call site's own
// properties (e.g. `ProductCard(name = "Latte", productId = 3)`),
// falling back to `declaredParams`' own defaults for any optional
// param the call site omitted. Skips "id" (that's the *cloned* node's
// own id, not an argument), the internal "__unresolvedImportCall"
// marker, and -- critically -- any name that is also one of the
// component's OWN declared `state` keys (`ownState`). A call-site
// argument becomes a frozen literal; state must stay a live VM-global
// reference so RefreshAll()/reactivity and RenameStateReferences keep
// working for it. Without this exclusion, a call site like
// `Counter(count = 0)` on a component that also declares
// `state count = 0` would silently bake `count` into the literal "0"
// everywhere, including inside the component's own click handlers
// that are supposed to increment it -- turning a reactive variable
// into dead text. When a collision happens the state declaration wins
// and the argument is dropped from the map; callers should treat
// call-site argument names and a component's own state names as one
// shared namespace and avoid reusing them.
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

// Same whole-identifier text substitution as RenameStateReferences,
// but replaces each match with a literal value instead of a renamed
// identifier, and walks starting at `node` itself (not just its
// children) so the clonedRoot's own properties get the same treatment
// as its descendants'. This is what lets a call-site argument be used
// anywhere in the component's own view/handlers -- including inside a
// handler call like `click = OnAddToCart(productId)`, which needs a
// real per-instance literal (`OnAddToCart(3)`) since every call site
// clones its own independent subtree and there is no per-instance VM
// scope to hold a shared `productId` global in.
void SubstituteCallArgs(IComponent* node,
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

    for (IComponent* child : node->Children()) {
        SubstituteCallArgs(child, valueMap);
    }
}

} // namespace

ComponentResolver::ComponentResolver(std::string projectRoot, std::string componentsDir)
    : projectRoot_(std::move(projectRoot)), componentsDir_(std::move(componentsDir)) {}

bool ComponentResolver::IsComponentCall(const IComponent* comp) {
    if (!comp) return false;
    // `slot()` is parsed with call syntax (see AvauiParser::ParseComponent),
    // so it also gets the generic "__unresolvedImportCall" marker like any
    // other Component(...) call site -- but it is a built-in layout
    // placeholder (RenderTree.cpp's `typeName == "Slot"` -> RenderNodeType::
    // Slot), not something with a components/Slot.avaui file to load. Left
    // unguarded, LoadComponent() below fails to find that file, the call
    // site resolves to zero children, and ResolveChildrenOf still deletes
    // the original node because `changed` was set -- silently dropping the
    // page's entire content wherever the layout put `slot()`. Treat it as
    // an ordinary node instead of a call site so it survives resolution
    // intact for SceneCommandWalker to splice the page fragment into.
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

        // With an alias, that alias IS the callable tag (the view calls
        // it as `dialog()`, not `ConfirmDialog()`) -- it fully replaces
        // the default PascalCase tag CallableTagFromDotted would derive,
        // matching ordinary `import ... as` shadowing semantics.
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
        // The cached parse always keeps this component's OWN, bare state
        // names (it's shared across every call site, aliased or not --
        // see the cache_ key, which is the file path, not the tag/alias);
        // namespacing only ever happens here, at merge time, per call
        // site, which is what lets two differently-aliased imports of
        // the very same file merge in two independently-prefixed copies.
        MergeStateMap(mergedState, cacheIt->second->parsed.state, alias);
        return cacheIt->second.get();
    }

    std::string source;
    if (!ReadFile(key, source)) return nullptr;

    parser::ParsedAvaui parsed;
    try {
        parsed = parser::AvauiParser::Parse(source);
    } catch (const parser::ParseError&) {
        return nullptr;
    }
    if (!parsed.tree) return nullptr;

    auto entry = std::make_unique<CacheEntry>();
    entry->mtime = mtime;
    entry->parsed = std::move(parsed);

    ImportMap ownImportMap = BuildImportMap(entry->parsed.imports);
    IComponent* root = entry->parsed.tree->Root();
    if (root) {
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
                                           const ImportMap& importMap, int depth) {
    if (!parent || !tree || depth > kMaxDepth) return;
    if (parent->TypeName() == "For") return;

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
            ResolveChildrenOf(c, tree, mergedState, importMap, depth + 1);
        }
    }
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
                                          std::unordered_map<std::string, std::string>& mergedState) {
    if (!tree) return;
    IComponent* root = tree->Root();
    if (!root) return;

    ImportMap importMap = BuildImportMap(imports);
    ResolveChildrenOf(root, tree, mergedState, importMap, 0);
}

} // namespace ui
} // namespace avalang
