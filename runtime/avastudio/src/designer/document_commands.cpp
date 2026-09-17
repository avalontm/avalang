#include "designer/document_commands.h"

#include <cctype>
#include <utility>

#include "designer/component_extraction.h"
#include "designer/component_extraction_command.h"
#include "designer/lifecycle_commands.h"
#include "designer/move_commands.h"
#include "designer/property_commands.h"
#include "resolver/DottedPath.h"

namespace studio::designer {

namespace {

std::string ToLowerAscii(const std::string& value) {
    std::string result = value;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

UiComponentTree* TreeOf(design::DesignDocument& document) {
    if (!document.tree || !document.Root()) {
        return nullptr;
    }
    return document.tree.get();
}

std::vector<std::pair<std::string, PropertyValue>> ToPropertyValues(
    const std::string& id, const std::vector<PropertyRow>& properties) {
    std::vector<std::pair<std::string, PropertyValue>> values;
    values.reserve(properties.size() + 1);
    if (!id.empty()) {
        values.emplace_back("id", PropertyValue(id));
    }
    for (const PropertyRow& row : properties) {
        values.emplace_back(row.key, PropertyValue(row.value));
    }
    return values;
}

}

DocumentCommand::DocumentCommand(design::DesignDocument* document, std::unique_ptr<ICommand> inner,
                                  SelectionManager* selection)
    : document_(document), inner_(std::move(inner)), selection_(selection) {}

void DocumentCommand::Execute() {
    if (inner_) {
        inner_->Execute();
    }
    SyncDocument();
}

void DocumentCommand::Undo() {
    if (inner_) {
        inner_->Undo();
    }
    SyncDocument();
}

void DocumentCommand::Redo() {
    if (inner_) {
        inner_->Redo();
    }
    SyncDocument();
}

std::string DocumentCommand::Description() const {
    return inner_ ? inner_->Description() : std::string();
}

void DocumentCommand::SyncDocument() {
    if (!document_) {
        return;
    }
    document_->dirty = true;
    if (!selection_) {
        return;
    }
    for (const NodeId& id : std::vector<NodeId>(selection_->Selected())) {
        if (!design::FindNodeById(document_->Root(), id)) {
            selection_->Deselect(id);
        }
    }
    if (!selection_->Hovered().empty() && !design::FindNodeById(document_->Root(), selection_->Hovered())) {
        selection_->ClearHovered();
    }
    if (!selection_->Focused().empty() && !design::FindNodeById(document_->Root(), selection_->Focused())) {
        selection_->ClearFocused();
    }
}

std::string ExecuteAddComponent(CommandManager* manager, design::DesignDocument& document,
                                 SelectionManager* selection, const std::string& parentNodeId,
                                 const std::string& type, const std::string& id,
                                 const std::vector<PropertyRow>& properties) {
    if (!manager) {
        return design::AddComponentNode(document, parentNodeId, type, id, properties);
    }

    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return {};
    }

    const std::string parentId = parentNodeId.empty() ? document.Root()->NodeId() : parentNodeId;
    if (!design::FindNodeById(document.Root(), parentId)) {
        return {};
    }

    auto command = std::make_unique<CreateComponentCommand>(tree, parentId, type,
                                                             ToPropertyValues(id, properties));
    CreateComponentCommand* created = command.get();
    manager->Execute(std::make_unique<DocumentCommand>(&document, std::move(command), selection));
    const std::string createdNodeId = created->CreatedNodeId();
    if (!createdNodeId.empty()) {
        if (!id.empty()) {
            design::MarkPropertyAuthored(document, createdNodeId, "id");
        }
        for (const PropertyRow& row : properties) {
            design::MarkPropertyAuthored(document, createdNodeId, row.key);
        }
    }
    return createdNodeId;
}

bool ExecuteMoveComponent(CommandManager* manager, design::DesignDocument& document,
                           SelectionManager* selection, const std::string& movedNodeId,
                           const std::string& targetNodeId, design::DropZone zone) {
    if (!manager) {
        return design::MoveNode(document, movedNodeId, targetNodeId, zone);
    }

    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return false;
    }
    if (movedNodeId == targetNodeId || document.Root()->NodeId() == movedNodeId) {
        return false;
    }

    UiNode* moved = design::FindNodeById(document.Root(), movedNodeId);
    UiNode* target = design::FindNodeById(document.Root(), targetNodeId);
    if (!moved || !target || design::NodeContains(moved, target)) {
        return false;
    }

    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<MoveComponentCommand>(tree, movedNodeId, targetNodeId, zone), selection));
    return true;
}

bool ExecuteRemoveComponent(CommandManager* manager, design::DesignDocument& document,
                             SelectionManager* selection, const std::string& nodeId) {
    if (!manager) {
        if (!design::RemoveNode(document, nodeId)) {
            return false;
        }
        if (selection) {
            selection->Deselect(nodeId);
        }
        return true;
    }

    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return false;
    }
    if (document.Root()->NodeId() == nodeId) {
        return false;
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node || !design::FindParentOf(document.Root(), node)) {
        return false;
    }

    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<DeleteComponentCommand>(tree, nodeId), selection));
    return true;
}

std::string ExecuteChangeComponentType(CommandManager* manager, design::DesignDocument& document,
                                        SelectionManager* selection, const std::string& nodeId,
                                        const std::string& newType) {
    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return {};
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node || !node->Parent()) {
        return {};
    }
    if (ToLowerAscii(node->TypeName()) == ToLowerAscii(newType)) {
        return {};
    }

    std::vector<std::string> authoredKeys;
    for (const std::string& name : node->PropertyNames()) {
        if (design::IsPropertyAuthored(document, nodeId, name)) {
            authoredKeys.push_back(name);
        }
    }

    auto command = std::make_unique<ChangeComponentTypeCommand>(tree, nodeId, newType);
    ChangeComponentTypeCommand* raw = command.get();

    if (manager) {
        manager->Execute(std::make_unique<DocumentCommand>(&document, std::move(command), selection));
    } else {
        raw->Execute();
        document.dirty = true;
    }

    const std::string changedId = raw->ChangedNodeId();
    if (!changedId.empty()) {
        for (const std::string& key : authoredKeys) {
            design::MarkPropertyAuthored(document, changedId, key);
        }
        if (selection) {
            selection->Deselect(nodeId);
            selection->Select(changedId, false);
        }
    }
    return changedId;
}

std::string ExecuteExtractComponent(CommandManager* manager, design::DesignDocument& document,
                                     SelectionManager* selection, const std::string& nodeId,
                                     const std::string& projectRoot, std::string* out_error) {
    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return {};
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node) {
        return {};
    }

    const ExtractionCandidate candidate = ValidateExtractionCandidate(tree, nodeId);
    if (!candidate.valid) {
        if (out_error) {
            *out_error = candidate.reason;
        }
        return {};
    }

    auto command = std::make_unique<ExtractComponentCommand>(
        tree, &document.imports, nodeId, projectRoot,
        avalang::ui::CallableTagFromDotted(TypeOf(node)));
    ExtractComponentCommand* raw = command.get();
    if (manager) {
        manager->Execute(std::make_unique<DocumentCommand>(&document, std::move(command), selection));
    } else {
        command->Execute();
    }

    if (!raw->Ok()) {
        if (out_error) {
            *out_error = raw->FailureReason();
        }
        return {};
    }
    return raw->CreatedNodeId();
}

bool ExecuteSetProperty(CommandManager* manager, design::DesignDocument& document,
                         SelectionManager* selection, const std::string& nodeId, const std::string& key,
                         const std::string& value) {
    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return false;
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node) {
        return false;
    }

    if (!manager) {
        node->SetProperty(key, PropertyValue(value));
        document.dirty = true;
        design::MarkPropertyAuthored(document, nodeId, key);
        return true;
    }

    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<SetPropertyCommand>(tree, nodeId, key, PropertyValue(value)),
        selection));
    design::MarkPropertyAuthored(document, nodeId, key);
    return true;
}

bool ExecuteRemoveProperty(CommandManager* manager, design::DesignDocument& document,
                            SelectionManager* selection, const std::string& nodeId, const std::string& key) {
    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return false;
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node) {
        return false;
    }

    if (!manager) {
        node->RemoveProperty(key);
        document.dirty = true;
        design::UnmarkPropertyAuthored(document, nodeId, key);
        return true;
    }

    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<RemovePropertyCommand>(tree, nodeId, key), selection));
    design::UnmarkPropertyAuthored(document, nodeId, key);
    return true;
}

}
