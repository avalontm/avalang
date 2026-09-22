#include "designer/lifecycle_commands.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>

#include "design/design_document.h"
#include "parser/AvauiPropertyCoercion.h"

namespace studio::designer {

namespace {

std::string CanonicalType(const std::string& type) {
    return avalang::ui::parser::CanonicalTypeName(type);
}

void DestroySubtree(UiComponentTree* tree, UiNode* node) {
    if (!tree || !node) {
        return;
    }
    for (UiNode* child : node->Children()) {
        DestroySubtree(tree, child);
    }
    tree->DestroyComponent(node->Id());
}

void CollectIds(UiNode* node, std::unordered_set<std::string>& out) {
    if (!node) {
        return;
    }
    if (const PropertyValue* idProp = node->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String) {
            out.insert(idProp->AsString());
        }
    }
    for (UiNode* child : node->Children()) {
        CollectIds(child, out);
    }
}

std::string UniqueId(UiNode* root, const std::string& base) {
    std::unordered_set<std::string> existing;
    CollectIds(root, existing);
    if (!existing.count(base)) {
        return base;
    }
    int suffix = 2;
    while (existing.count(base + "_" + std::to_string(suffix))) {
        ++suffix;
    }
    return base + "_" + std::to_string(suffix);
}

UiNode* CloneSubtree(UiComponentTree* tree, UiNode* root, UiNode* source) {
    UiNode* clone = tree->CreateComponent(source->TypeName());

    for (const std::string& name : source->PropertyNames()) {
        if (name == "id") {
            continue;
        }
        if (const PropertyValue* value = source->GetProperty(name)) {
            clone->SetProperty(name, *value);
        }
    }

    if (const PropertyValue* idProp = source->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
            clone->SetProperty("id", PropertyValue(UniqueId(root, idProp->AsString())));
        }
    }

    for (UiNode* child : source->Children()) {
        clone->AddChild(CloneSubtree(tree, root, child));
    }

    return clone;
}

}

CreateComponentCommand::CreateComponentCommand(
    UiComponentTree* tree, NodeId parentId, ComponentTypeId type,
    std::vector<std::pair<std::string, PropertyValue>> properties)
    : tree_(tree),
      parentId_(std::move(parentId)),
      type_(CanonicalType(type)),
      properties_(std::move(properties)) {}

CreateComponentCommand::~CreateComponentCommand() {
    if (tree_ && node_ && !node_->Parent()) {
        DestroySubtree(tree_, node_);
    }
}

void CreateComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    if (!node_) {
        node_ = tree_->CreateComponent(type_);
        for (const auto& property : properties_) {
            node_->SetProperty(property.first, property.second);
        }
    }
    Attach();
}

void CreateComponentCommand::Redo() {
    Attach();
}

void CreateComponentCommand::Attach() {
    if (!tree_ || !node_) {
        return;
    }
    if (UiNode* parent = studio::design::FindNodeById(tree_->Root(), parentId_)) {
        parent->AddChild(node_);
    }
}

void CreateComponentCommand::Undo() {
    if (!node_) {
        return;
    }
    if (UiNode* parent = node_->Parent()) {
        parent->RemoveChild(node_);
    }
}

std::string CreateComponentCommand::Description() const {
    return "Create " + type_;
}

NodeId CreateComponentCommand::CreatedNodeId() const {
    return node_ ? IdOf(node_) : NodeId();
}

InsertComponentCommand::InsertComponentCommand(
    UiComponentTree* tree, NodeId targetId, studio::design::DropZone zone, ComponentTypeId type,
    std::vector<std::pair<std::string, PropertyValue>> properties)
    : tree_(tree),
      targetId_(std::move(targetId)),
      zone_(zone),
      type_(CanonicalType(type)),
      properties_(std::move(properties)) {}

InsertComponentCommand::~InsertComponentCommand() {
    if (tree_ && node_ && !node_->Parent()) {
        DestroySubtree(tree_, node_);
    }
}

void InsertComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    if (!node_) {
        node_ = tree_->CreateComponent(type_);
        for (const auto& property : properties_) {
            node_->SetProperty(property.first, property.second);
        }
    }
    Attach();
}

void InsertComponentCommand::Redo() {
    Attach();
}

void InsertComponentCommand::Attach() {
    if (!tree_ || !node_) {
        return;
    }
    UiNode* root = tree_->Root();
    UiNode* target = studio::design::FindNodeById(root, targetId_);
    if (!target) {
        return;
    }
    UiNode* parent = zone_ == studio::design::DropZone::kInto ? target : target->Parent();
    if (!parent) {
        return;
    }
    parent->AddChild(node_);
    if (zone_ != studio::design::DropZone::kInto) {
        studio::design::MoveNode(root, IdOf(node_), targetId_, zone_);
    }
}

void InsertComponentCommand::Undo() {
    if (!node_) {
        return;
    }
    if (UiNode* parent = node_->Parent()) {
        parent->RemoveChild(node_);
    }
}

std::string InsertComponentCommand::Description() const {
    return "Insert " + type_;
}

NodeId InsertComponentCommand::CreatedNodeId() const {
    return node_ ? IdOf(node_) : NodeId();
}

ChangeComponentTypeCommand::ChangeComponentTypeCommand(UiComponentTree* tree, NodeId nodeId,
                                                         ComponentTypeId newType)
    : tree_(tree), nodeId_(std::move(nodeId)), newType_(CanonicalType(newType)) {}

ChangeComponentTypeCommand::~ChangeComponentTypeCommand() {
    if (!tree_) {
        return;
    }
    if (oldNode_ && !oldNode_->Parent()) {
        DestroySubtree(tree_, oldNode_);
    }
    if (newNode_ && !newNode_->Parent()) {
        DestroySubtree(tree_, newNode_);
    }
}

void ChangeComponentTypeCommand::Swap(UiNode* from, UiNode* to) {
    if (!tree_ || !parent_ || !from || !to) {
        return;
    }

    for (const std::string& slotName : std::vector<std::string>(from->SlotNames())) {
        for (UiNode* child : std::vector<UiNode*>(from->SlotChildren(slotName))) {
            from->RemoveChild(child);
            to->AddChild(child, slotName);
        }
    }

    std::string ownSlot = "default";
    std::vector<UiNode*> siblings;
    for (const std::string& slotName : parent_->SlotNames()) {
        const std::vector<UiNode*>& slotChildren = parent_->SlotChildren(slotName);
        if (std::find(slotChildren.begin(), slotChildren.end(), from) != slotChildren.end()) {
            ownSlot = slotName;
            siblings = slotChildren;
            break;
        }
    }

    std::vector<UiNode*> reordered;
    reordered.reserve(siblings.size());
    for (UiNode* sibling : siblings) {
        reordered.push_back(sibling == from ? to : sibling);
    }

    for (UiNode* sibling : siblings) {
        parent_->RemoveChild(sibling);
    }
    for (UiNode* sibling : reordered) {
        parent_->AddChild(sibling, ownSlot);
    }
}

void ChangeComponentTypeCommand::Execute() {
    if (!tree_) {
        return;
    }
    if (!oldNode_) {
        oldNode_ = studio::design::FindNodeById(tree_->Root(), nodeId_);
        if (!oldNode_) {
            return;
        }
        parent_ = oldNode_->Parent();
        if (!parent_) {
            oldNode_ = nullptr;
            return;
        }
    }
    if (!newNode_) {
        newNode_ = tree_->CreateComponent(newType_);
        for (const std::string& name : oldNode_->PropertyNames()) {
            if (const PropertyValue* value = oldNode_->GetProperty(name)) {
                newNode_->SetProperty(name, *value);
            }
        }
    }
    Swap(oldNode_, newNode_);
}

void ChangeComponentTypeCommand::Redo() {
    Swap(oldNode_, newNode_);
}

void ChangeComponentTypeCommand::Undo() {
    Swap(newNode_, oldNode_);
}

std::string ChangeComponentTypeCommand::Description() const {
    return "Change type to " + newType_;
}

NodeId ChangeComponentTypeCommand::ChangedNodeId() const {
    return newNode_ ? IdOf(newNode_) : NodeId();
}

std::pair<std::string, std::string> ChangeComponentTypeCommand::IdentitySwap() const {
    if (!newNode_) {
        return {};
    }
    return {nodeId_, IdOf(newNode_)};
}

DeleteComponentCommand::DeleteComponentCommand(UiComponentTree* tree, NodeId nodeId)
    : tree_(tree), nodeId_(std::move(nodeId)) {}

DeleteComponentCommand::~DeleteComponentCommand() {
    if (tree_ && node_ && !node_->Parent()) {
        DestroySubtree(tree_, node_);
    }
}

void DeleteComponentCommand::CaptureLocation() {
    slot_ = "default";
    nextSiblingId_.clear();
    if (!parent_ || !node_) {
        return;
    }
    for (const std::string& slotName : parent_->SlotNames()) {
        const std::vector<UiNode*>& siblings = parent_->SlotChildren(slotName);
        const auto it = std::find(siblings.begin(), siblings.end(), node_);
        if (it == siblings.end()) {
            continue;
        }
        slot_ = slotName;
        const auto next = std::next(it);
        if (next != siblings.end()) {
            nextSiblingId_ = IdOf(*next);
        }
        return;
    }
}

void DeleteComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    UiNode* root = tree_->Root();
    if (root && IdOf(root) == nodeId_) {
        return;
    }
    if (!node_) {
        node_ = studio::design::FindNodeById(root, nodeId_);
        if (!node_) {
            return;
        }
        parent_ = node_->Parent();
        CaptureLocation();
    }
    if (parent_) {
        parent_->RemoveChild(node_);
    }
}

void DeleteComponentCommand::Undo() {
    if (!tree_ || !parent_ || !node_) {
        return;
    }
    parent_->AddChild(node_, slot_);
    if (!nextSiblingId_.empty()) {
        studio::design::MoveNode(tree_->Root(), IdOf(node_), nextSiblingId_, studio::design::DropZone::kBefore);
    }
}

void DeleteComponentCommand::Redo() {
    if (parent_ && node_) {
        parent_->RemoveChild(node_);
    }
}

std::string DeleteComponentCommand::Description() const {
    return "Delete " + nodeId_;
}

PasteComponentCommand::PasteComponentCommand(UiComponentTree* tree, NodeId sourceId, NodeId targetParentId)
    : tree_(tree), sourceId_(std::move(sourceId)), targetParentId_(std::move(targetParentId)) {}

PasteComponentCommand::~PasteComponentCommand() {
    if (tree_ && clone_ && !clone_->Parent()) {
        DestroySubtree(tree_, clone_);
    }
}

void PasteComponentCommand::Attach() {
    if (!tree_ || !clone_) {
        return;
    }
    UiNode* root = tree_->Root();
    UiNode* parent = studio::design::FindNodeById(root, targetParentId_);
    if (!parent) {
        return;
    }
    parent->AddChild(clone_);
    if (!afterId_.empty()) {
        studio::design::MoveNode(root, IdOf(clone_), afterId_, studio::design::DropZone::kAfter);
    }
}

void PasteComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    if (!clone_) {
        UiNode* root = tree_->Root();
        if (!studio::design::FindNodeById(root, targetParentId_)) {
            return;
        }
        UiNode* source = studio::design::FindNodeById(root, sourceId_);
        if (!source) {
            return;
        }
        clone_ = CloneSubtree(tree_, root, source);
    }
    Attach();
}

void PasteComponentCommand::Redo() {
    Attach();
}

void PasteComponentCommand::Undo() {
    if (!clone_) {
        return;
    }
    if (UiNode* parent = clone_->Parent()) {
        parent->RemoveChild(clone_);
    }
}

std::string PasteComponentCommand::Description() const {
    return "Paste " + sourceId_;
}

NodeId PasteComponentCommand::PastedNodeId() const {
    return clone_ ? IdOf(clone_) : NodeId();
}

DuplicateComponentCommand::DuplicateComponentCommand(UiComponentTree* tree, NodeId sourceId)
    : PasteComponentCommand(tree, std::move(sourceId), NodeId()) {}

void DuplicateComponentCommand::Execute() {
    if (!tree_) {
        return;
    }
    if (UiNode* source = studio::design::FindNodeById(tree_->Root(), sourceId_)) {
        if (UiNode* parent = source->Parent()) {
            targetParentId_ = IdOf(parent);
            afterId_ = sourceId_;
        }
    }
    PasteComponentCommand::Execute();
}

std::string DuplicateComponentCommand::Description() const {
    return "Duplicate " + sourceId_;
}

}