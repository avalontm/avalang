#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "designer/command.h"
#include "designer/types.h"

namespace studio::designer {

class ExtractComponentCommand : public ICommand {
public:
    ExtractComponentCommand(UiComponentTree* tree, std::vector<std::string>* imports, NodeId nodeId,
                            std::string projectRoot, std::string componentName);
    ~ExtractComponentCommand() override;

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;
    std::pair<std::string, std::string> IdentitySwap() const override;

    bool Ok() const;
    NodeId CreatedNodeId() const;
    const std::string& FailureReason() const;

private:
    bool PrepareDefinition();
    bool WriteFile() const;
    void RemoveFile() const;
    void ReplaceNodeWithCallSite();
    void AppendImport();

    UiComponentTree* tree_;
    std::vector<std::string>* imports_;
    NodeId nodeId_;
    std::string projectRoot_;
    std::string componentName_;
    std::string filePath_;
    std::string importLine_;
    std::vector<std::string> definitionImports_;
    std::unique_ptr<avalang::ui::ComponentTree> definitionTree_;
    std::string definitionContent_;
    std::string failureReason_;
    UiNode* node_ = nullptr;
    UiNode* replacement_ = nullptr;
    UiNode* parent_ = nullptr;
    bool definitionPrepared_ = false;
    bool fileWritten_ = false;
    bool committed_ = false;
};

}