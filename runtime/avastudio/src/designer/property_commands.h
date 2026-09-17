#pragma once

#include <string>

#include "designer/command.h"
#include "designer/types.h"

namespace studio::designer {

class SetPropertyCommand : public ICommand {
public:
    SetPropertyCommand(UiComponentTree* tree, NodeId nodeId, std::string property,
                        PropertyValue newValue);

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

protected:
    UiComponentTree* tree_;
    NodeId nodeId_;
    std::string property_;
    PropertyValue newValue_;

private:
    void Apply(bool hasValue, const PropertyValue& value);

    bool hadOldValue_ = false;
    PropertyValue oldValue_;
};

class RemovePropertyCommand : public ICommand {
public:
    RemovePropertyCommand(UiComponentTree* tree, NodeId nodeId, std::string property);

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

private:
    void Remove();

    UiComponentTree* tree_;
    NodeId nodeId_;
    std::string property_;
    bool hadOldValue_ = false;
    PropertyValue oldValue_;
};

class SetStyleCommand : public SetPropertyCommand {
public:
    SetStyleCommand(UiComponentTree* tree, NodeId nodeId, std::string styleName);

    std::string Description() const override;
};

class SetEventCommand : public SetPropertyCommand {
public:
    SetEventCommand(UiComponentTree* tree, NodeId nodeId, std::string eventName, std::string handlerName);

    std::string Description() const override;
};

}
