#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "design/design_document.h"
#include "designer/command.h"
#include "designer/selection_manager.h"
#include "designer/types.h"
#include "panels/properties_panel.h"

namespace studio::designer {

struct AuthoredChange {
    NodeId nodeId;
    std::string key;
    bool before = false;
    bool after = false;
};

class DocumentCommand : public ICommand {
public:
    DocumentCommand(design::DesignDocument* document, std::unique_ptr<ICommand> inner,
                     SelectionManager* selection, std::vector<AuthoredChange> authored = {});

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

private:
    void ApplyAuthored(bool useAfter);
    void SyncDocument();
    std::string RemapId(const std::string& id, const std::pair<std::string, std::string>& swap) const;

    design::DesignDocument* document_;
    std::unique_ptr<ICommand> inner_;
    SelectionManager* selection_;
    std::vector<AuthoredChange> authored_;
};

std::string ExecuteAddComponent(CommandManager* manager, design::DesignDocument& document,
                                 SelectionManager* selection, const std::string& parentNodeId,
                                 const std::string& type, const std::string& id,
                                 const std::vector<PropertyRow>& properties);

bool ExecuteMoveComponent(CommandManager* manager, design::DesignDocument& document,
                           SelectionManager* selection, const std::string& movedNodeId,
                           const std::string& targetNodeId, design::DropZone zone);

bool ExecuteRemoveComponent(CommandManager* manager, design::DesignDocument& document,
                             SelectionManager* selection, const std::string& nodeId);

std::string ExecuteDuplicateComponent(CommandManager* manager, design::DesignDocument& document,
                                       SelectionManager* selection, const std::string& nodeId);

std::string ExecuteChangeComponentType(CommandManager* manager, design::DesignDocument& document,
                                        SelectionManager* selection, const std::string& nodeId,
                                        const std::string& newType);

std::string ExecuteExtractComponent(CommandManager* manager, design::DesignDocument& document,
                                     SelectionManager* selection, const std::string& nodeId,
                                     const std::string& projectRoot, std::string* out_error = nullptr);

bool ExecuteSetProperty(CommandManager* manager, design::DesignDocument& document,
                         SelectionManager* selection, const std::string& nodeId, const std::string& key,
                         const std::string& value);

bool ExecuteRemoveProperty(CommandManager* manager, design::DesignDocument& document,
                            SelectionManager* selection, const std::string& nodeId, const std::string& key);

}
