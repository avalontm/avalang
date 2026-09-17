#pragma once

#include <string>

#include "design/design_document.h"
#include "designer/command.h"
#include "designer/types.h"

namespace studio::designer {

class MoveComponentCommand : public ICommand {
public:
    MoveComponentCommand(UiComponentTree* tree, NodeId movedId, NodeId targetId, studio::design::DropZone zone);

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

protected:
    UiComponentTree* tree_;
    NodeId movedId_;
    NodeId targetId_;

private:
    studio::design::DropZone zone_;
    NodeId oldParentId_;
    NodeId oldNextSiblingId_;
    bool captured_ = false;
};

class ReparentComponentCommand : public MoveComponentCommand {
public:
    ReparentComponentCommand(UiComponentTree* tree, NodeId nodeId, NodeId newParentId);

    std::string Description() const override;
};

}
