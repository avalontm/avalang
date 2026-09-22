#include "designer/component_extraction_command.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "design/design_document.h"
#include "parser/AvauiWriter.h"
#include "resolver/DottedPath.h"

namespace studio::designer {

namespace {

void DestroySubtree(UiComponentTree* tree, UiNode* node) {
    if (!tree || !node) {
        return;
    }
    for (UiNode* child : node->Children()) {
        DestroySubtree(tree, child);
    }
    tree->DestroyComponent(node->Id());
}

UiNode* CloneSubtree(UiComponentTree* tree, const UiNode* source) {
    UiNode* clone = tree->CreateComponent(source->TypeName());
    for (const std::string& name : source->PropertyNames()) {
        if (const PropertyValue* value = source->GetProperty(name)) {
            clone->SetProperty(name, *value);
        }
    }
    for (const UiNode* child : source->Children()) {
        clone->AddChild(CloneSubtree(tree, child));
    }
    return clone;
}

void CollectCallSiteTags(const UiNode* node, std::unordered_set<std::string>& out) {
    if (!node) {
        return;
    }
    if (const PropertyValue* call = node->GetProperty("__unresolvedImportCall")) {
        if (call->Type() == PropertyType::Bool && call->AsBool()) {
            out.insert(node->TypeName());
        }
    }
    for (const UiNode* child : node->Children()) {
        CollectCallSiteTags(child, out);
    }
}

std::string ImportTag(const std::string& raw) {
    std::istringstream iss(raw);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() >= 3 && tokens[tokens.size() - 2] == "as") {
        return tokens.back();
    }
    std::string dotted;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i != 0) {
            dotted += " ";
        }
        dotted += tokens[i];
    }
    return avalang::ui::CallableTagFromDotted(dotted);
}

std::vector<std::string> ReferencedImports(const UiNode* node, const std::vector<std::string>& imports) {
    std::unordered_set<std::string> tags;
    CollectCallSiteTags(node, tags);
    if (tags.empty()) {
        return {};
    }

    std::vector<std::string> referenced;
    for (const std::string& raw : imports) {
        if (tags.count(ImportTag(raw)) != 0) {
            referenced.push_back(raw);
        }
    }
    return referenced;
}

UiNode* NextSibling(const UiNode* parent, const UiNode* node) {
    if (!parent || !node) {
        return nullptr;
    }
    const std::vector<UiNode*> children = parent->Children();
    auto it = std::find(children.begin(), children.end(), node);
    if (it == children.end() || std::next(it) == children.end()) {
        return nullptr;
    }
    return *std::next(it);
}

std::filesystem::path DefinitionPath(const std::string& projectRoot, const std::string& nativeName) {
    return std::filesystem::path(projectRoot) / "components" / (nativeName + ".avaui");
}

}

ExtractComponentCommand::ExtractComponentCommand(UiComponentTree* tree, std::vector<std::string>* imports,
                                                 NodeId nodeId, std::string projectRoot,
                                                 std::string componentName)
    : tree_(tree),
      imports_(imports),
      nodeId_(std::move(nodeId)),
      projectRoot_(std::move(projectRoot)),
      componentName_(std::move(componentName)) {
    std::string candidate = componentName_;
    int suffix = 1;
    while (std::filesystem::exists(DefinitionPath(projectRoot_, candidate))) {
        candidate = componentName_ + std::to_string(suffix);
        ++suffix;
    }
    componentName_ = std::move(candidate);
    filePath_ = DefinitionPath(projectRoot_, componentName_).string();
    importLine_ = "components." + componentName_;
}

ExtractComponentCommand::~ExtractComponentCommand() {
    if (tree_ && node_ && !node_->Parent()) {
        DestroySubtree(tree_, node_);
    }
    if (tree_ && replacement_ && !replacement_->Parent()) {
        DestroySubtree(tree_, replacement_);
    }
}

void ExtractComponentCommand::Execute() {
    if (!tree_ || !imports_) {
        return;
    }

    if (!node_) {
        node_ = design::FindNodeById(tree_->Root(), nodeId_);
        if (!node_) {
            failureReason_ = "Node not found";
            return;
        }
    }

    if (!parent_) {
        parent_ = node_->Parent();
        if (!parent_) {
            failureReason_ = "The document root cannot be extracted";
            return;
        }
    }

    if (!definitionPrepared_) {
        if (!PrepareDefinition()) {
            failureReason_ = "Could not serialize component definition";
            return;
        }
        definitionPrepared_ = true;
    }

    if (!fileWritten_) {
        if (!WriteFile()) {
            failureReason_ = "Could not write " + filePath_;
            return;
        }
        fileWritten_ = true;
    }

    ReplaceNodeWithCallSite();
    AppendImport();
    committed_ = true;
}

void ExtractComponentCommand::Undo() {
    if (!committed_) {
        return;
    }

    if (imports_) {
        auto it = std::find(imports_->begin(), imports_->end(), importLine_);
        if (it != imports_->end()) {
            imports_->erase(it);
        }
    }

    if (fileWritten_) {
        RemoveFile();
        fileWritten_ = false;
    }

    if (replacement_ && parent_) {
        parent_->RemoveChild(replacement_);
    }
    if (node_ && parent_) {
        parent_->AddChild(node_);
    }
    committed_ = false;
}

void ExtractComponentCommand::Redo() {
    Execute();
}

std::string ExtractComponentCommand::Description() const {
    return "Extract " + componentName_;
}

bool ExtractComponentCommand::Ok() const {
    return committed_;
}

NodeId ExtractComponentCommand::CreatedNodeId() const {
    return replacement_ ? IdOf(replacement_) : NodeId();
}

std::pair<std::string, std::string> ExtractComponentCommand::IdentitySwap() const {
    if (!replacement_) {
        return {};
    }
    return {nodeId_, IdOf(replacement_)};
}

const std::string& ExtractComponentCommand::FailureReason() const {
    return failureReason_;
}

bool ExtractComponentCommand::PrepareDefinition() {
    definitionImports_ = ReferencedImports(node_, *imports_);

    definitionTree_ = avalang::ui::ComponentTree::Create();
    UiNode* page = definitionTree_->CreateComponent("Page");
    definitionTree_->SetRoot(page);

    UiNode* definition = CloneSubtree(definitionTree_.get(), node_);
    definition->RemoveProperty("id");
    page->AddChild(definition);

    avalang::ui::parser::AvauiWriteOptions options;
    options.imports = definitionImports_;
    definitionContent_ = avalang::ui::parser::WriteAvaui(page, options);
    return true;
}

bool ExtractComponentCommand::WriteFile() const {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(filePath_).parent_path(), ec);
    std::ofstream out(filePath_, std::ios::binary);
    if (!out) {
        return false;
    }
    out << definitionContent_;
    return out.good();
}

void ExtractComponentCommand::RemoveFile() const {
    std::error_code ec;
    std::filesystem::remove(filePath_, ec);
}

void ExtractComponentCommand::ReplaceNodeWithCallSite() {
    if (!node_ || !parent_ || !tree_) {
        return;
    }

    UiNode* anchor = NextSibling(parent_, node_);
    parent_->RemoveChild(node_);

    if (!replacement_) {
        replacement_ = tree_->CreateComponent(componentName_);
        replacement_->SetProperty("__unresolvedImportCall", PropertyValue(true));
        if (const PropertyValue* id = node_->GetProperty("id")) {
            replacement_->SetProperty("id", *id);
        }
    }
    parent_->AddChild(replacement_);

    if (anchor) {
        design::MoveNode(tree_->Root(), IdOf(replacement_), IdOf(anchor), design::DropZone::kBefore);
    }
}

void ExtractComponentCommand::AppendImport() {
    if (importLine_.empty()) {
        return;
    }
    if (std::find(imports_->begin(), imports_->end(), importLine_) == imports_->end()) {
        imports_->push_back(importLine_);
    }
}

}