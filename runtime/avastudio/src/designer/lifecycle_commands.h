#pragma once

#include <string>
#include <utility>
#include <vector>

#include "design/design_document.h"
#include "designer/command.h"
#include "designer/types.h"

namespace studio::designer {

class CreateComponentCommand : public ICommand {
public:
    CreateComponentCommand(UiComponentTree* tree, NodeId parentId, ComponentTypeId type,
                            std::vector<std::pair<std::string, PropertyValue>> properties = {});
    ~CreateComponentCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

    NodeId CreatedNodeId() const;
    std::string AffectedNodeId() const override { return CreatedNodeId(); }

private:
    void Attach();

    UiComponentTree* tree_;
    NodeId parentId_;
    ComponentTypeId type_;
    std::vector<std::pair<std::string, PropertyValue>> properties_;
    UiNode* node_ = nullptr;
};

class InsertComponentCommand : public ICommand {
public:
    InsertComponentCommand(UiComponentTree* tree, NodeId targetId, studio::design::DropZone zone,
                            ComponentTypeId type,
                            std::vector<std::pair<std::string, PropertyValue>> properties = {});
    ~InsertComponentCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

    NodeId CreatedNodeId() const;
    std::string AffectedNodeId() const override { return CreatedNodeId(); }

private:
    void Attach();

    UiComponentTree* tree_;
    NodeId targetId_;
    studio::design::DropZone zone_;
    ComponentTypeId type_;
    std::vector<std::pair<std::string, PropertyValue>> properties_;
    UiNode* node_ = nullptr;
};

class ChangeComponentTypeCommand : public ICommand {
public:
    ChangeComponentTypeCommand(UiComponentTree* tree, NodeId nodeId, ComponentTypeId newType);
    ~ChangeComponentTypeCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;
    std::pair<std::string, std::string> IdentitySwap() const override;

    NodeId ChangedNodeId() const;

private:
    void Swap(UiNode* from, UiNode* to);

    UiComponentTree* tree_;
    NodeId nodeId_;
    ComponentTypeId newType_;
    UiNode* oldNode_ = nullptr;
    UiNode* newNode_ = nullptr;
    UiNode* parent_ = nullptr;
};

class DeleteComponentCommand : public ICommand {
public:
    DeleteComponentCommand(UiComponentTree* tree, NodeId nodeId);
    ~DeleteComponentCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;
    std::string AffectedNodeId() const override { return nodeId_; }

private:
    void CaptureLocation();

    UiComponentTree* tree_;
    NodeId nodeId_;
    UiNode* node_ = nullptr;
    UiNode* parent_ = nullptr;
    std::string slot_ = "default";
    NodeId nextSiblingId_;
};

class PasteComponentCommand : public ICommand {
public:
    PasteComponentCommand(UiComponentTree* tree, NodeId sourceId, NodeId targetParentId);
    ~PasteComponentCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

    NodeId PastedNodeId() const;
    std::string AffectedNodeId() const override { return PastedNodeId(); }

protected:
    void Attach();

    UiComponentTree* tree_;
    NodeId sourceId_;
    NodeId targetParentId_;
    NodeId afterId_;
    UiNode* clone_ = nullptr;
};

class DuplicateComponentCommand : public PasteComponentCommand {
public:
    DuplicateComponentCommand(UiComponentTree* tree, NodeId sourceId);

    void Execute() override;
    std::string Description() const override;
};

}
