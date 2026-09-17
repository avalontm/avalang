#pragma once

#include <memory>
#include <string>
#include <vector>

namespace studio::designer {

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;

    virtual std::string Description() const = 0;
};

class CompositeCommand : public ICommand {
public:
    CompositeCommand(std::string description, std::vector<std::unique_ptr<ICommand>> commands);

    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string Description() const override;

private:
    std::string description_;
    std::vector<std::unique_ptr<ICommand>> commands_;
};

class CommandManager {
public:
    void Execute(std::unique_ptr<ICommand> command);

    bool CanUndo() const;
    bool CanRedo() const;

    void Undo();
    void Redo();

    void Clear();

    void BeginTransaction(const std::string& description);
    void EndTransaction();
    bool InTransaction() const;

private:
    std::vector<std::unique_ptr<ICommand>> undo_stack_;
    std::vector<std::unique_ptr<ICommand>> redo_stack_;

    std::string transaction_description_;
    std::vector<std::unique_ptr<ICommand>> transaction_commands_;
    bool in_transaction_ = false;
};

}
