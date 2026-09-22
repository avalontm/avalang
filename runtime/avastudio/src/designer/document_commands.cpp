#include "designer/document_commands.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "designer/component_extraction.h"
#include "designer/component_extraction_command.h"
#include "designer/lifecycle_commands.h"
#include "designer/move_commands.h"
#include "designer/property_catalog.h"
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

void CopyAuthoredProperties(design::DesignDocument& document, UiNode* source, UiNode* copy) {
    if (!source || !copy) {
        return;
    }
    for (const std::string& name : source->PropertyNames()) {
        if (design::IsPropertyAuthored(document, source->NodeId(), name)) {
            design::MarkPropertyAuthored(document, copy->NodeId(), name);
        }
    }
    const std::vector<UiNode*> sourceChildren = source->Children();
    const std::vector<UiNode*> copyChildren = copy->Children();
    const size_t count = std::min(sourceChildren.size(), copyChildren.size());
    for (size_t i = 0; i < count; ++i) {
        CopyAuthoredProperties(document, sourceChildren[i], copyChildren[i]);
    }
}

}

DocumentCommand::DocumentCommand(design::DesignDocument* document, std::unique_ptr<ICommand> inner,
                                  SelectionManager* selection, std::vector<AuthoredChange> authored)
    : document_(document), inner_(std::move(inner)), selection_(selection), authored_(std::move(authored)) {}

void DocumentCommand::Execute() {
    if (inner_) {
        inner_->Execute();
    }
    ApplyAuthored(true);
    SyncDocument();
}

void DocumentCommand::Undo() {
    if (inner_) {
        inner_->Undo();
    }
    ApplyAuthored(false);
    SyncDocument();
}

void DocumentCommand::Redo() {
    if (inner_) {
        inner_->Redo();
    }
    ApplyAuthored(true);
    SyncDocument();
}

void DocumentCommand::ApplyAuthored(bool useAfter) {
    if (!document_) {
        return;
    }
    for (const AuthoredChange& change : authored_) {
        if (useAfter ? change.after : change.before) {
            design::MarkPropertyAuthored(*document_, change.nodeId, change.key);
        } else {
            design::UnmarkPropertyAuthored(*document_, change.nodeId, change.key);
        }
    }
}

std::string DocumentCommand::Description() const {
    return inner_ ? inner_->Description() : std::string();
}

std::string DocumentCommand::RemapId(const std::string& id, const std::pair<std::string, std::string>& swap) const {
    if (id.empty() || design::FindNodeById(document_->Root(), id)) {
        return id;
    }
    if (!swap.first.empty() && id == swap.first && design::FindNodeById(document_->Root(), swap.second)) {
        return swap.second;
    }
    if (!swap.second.empty() && id == swap.second && design::FindNodeById(document_->Root(), swap.first)) {
        return swap.first;
    }
    return {};
}

void DocumentCommand::SyncDocument() {
    if (!document_) {
        return;
    }
    document_->dirty = true;
    document_->revision++;
    if (!selection_) {
        return;
    }

    const std::pair<std::string, std::string> swap = inner_ ? inner_->IdentitySwap() : std::pair<std::string, std::string>{};

    std::vector<NodeId> remappedSelection;
    for (const NodeId& id : selection_->Selected()) {
        const std::string remapped = RemapId(id, swap);
        if (!remapped.empty()) {
            remappedSelection.push_back(remapped);
        }
    }
    selection_->SelectMany(remappedSelection);

    const std::string remappedHovered = RemapId(selection_->Hovered(), swap);
    if (remappedHovered.empty()) {
        selection_->ClearHovered();
    } else if (remappedHovered != selection_->Hovered()) {
        selection_->SetHovered(remappedHovered);
    }

    const std::string remappedFocused = RemapId(selection_->Focused(), swap);
    if (remappedFocused.empty()) {
        selection_->ClearFocused();
    } else if (remappedFocused != selection_->Focused()) {
        selection_->SetFocused(remappedFocused);
    }

    if (selection_->IsEmpty() && inner_) {
        const std::string affected = inner_->AffectedNodeId();
        if (!affected.empty() && design::FindNodeById(document_->Root(), affected)) {
            selection_->Select(affected);
        }
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

std::string ExecuteDuplicateComponent(CommandManager* manager, design::DesignDocument& document,
                                       SelectionManager* selection, const std::string& nodeId) {
    UiComponentTree* tree = TreeOf(document);
    if (!tree) {
        return {};
    }

    UiNode* node = design::FindNodeById(document.Root(), nodeId);
    if (!node || node == document.Root() || !node->Parent()) {
        return {};
    }

    auto command = std::make_unique<DuplicateComponentCommand>(tree, nodeId);
    DuplicateComponentCommand* raw = command.get();
    if (manager) {
        manager->Execute(std::make_unique<DocumentCommand>(&document, std::move(command), selection));
    } else {
        raw->Execute();
        document.dirty = true;
        document.revision++;
    }

    const std::string duplicatedId = raw->PastedNodeId();
    if (!duplicatedId.empty()) {
        CopyAuthoredProperties(document, node, design::FindNodeById(document.Root(), duplicatedId));
    }
    return duplicatedId;
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
        document.revision++;
    }

    const std::string changedId = raw->ChangedNodeId();
    if (!changedId.empty()) {
        for (const std::string& key : authoredKeys) {
            design::MarkPropertyAuthored(document, changedId, key);
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

    const PropertyValue typedValue = ResolveTypedPropertyValue(node, key, value);

    if (!manager) {
        node->SetProperty(key, typedValue);
        document.dirty = true;
        document.revision++;
        design::MarkPropertyAuthored(document, nodeId, key);
        return true;
    }

    const bool wasAuthored = design::IsPropertyAuthored(document, nodeId, key);
    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<SetPropertyCommand>(tree, nodeId, key, typedValue), selection,
        std::vector<AuthoredChange>{{nodeId, key, wasAuthored, true}}));
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
        document.revision++;
        design::UnmarkPropertyAuthored(document, nodeId, key);
        return true;
    }

    const bool wasAuthored = design::IsPropertyAuthored(document, nodeId, key);
    manager->Execute(std::make_unique<DocumentCommand>(
        &document, std::make_unique<RemovePropertyCommand>(tree, nodeId, key), selection,
        std::vector<AuthoredChange>{{nodeId, key, wasAuthored, false}}));
    return true;
}

}
