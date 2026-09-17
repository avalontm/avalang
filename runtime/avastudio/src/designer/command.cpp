#include "designer/command.h"

#include <utility>

namespace studio::designer {

CompositeCommand::CompositeCommand(std::string description, std::vector<std::unique_ptr<ICommand>> commands)
    : description_(std::move(description)), commands_(std::move(commands)) {}

void CompositeCommand::Execute() {
    for (auto& command : commands_) {
        command->Execute();
    }
}

void CompositeCommand::Undo() {
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        (*it)->Undo();
    }
}

void CompositeCommand::Redo() {
    for (auto& command : commands_) {
        command->Redo();
    }
}

std::string CompositeCommand::Description() const {
    return description_;
}

void CommandManager::Execute(std::unique_ptr<ICommand> command) {
    if (!command) {
        return;
    }

    command->Execute();

    if (in_transaction_) {
        transaction_commands_.push_back(std::move(command));
        return;
    }

    undo_stack_.push_back(std::move(command));
    redo_stack_.clear();
}

bool CommandManager::CanUndo() const {
    return !undo_stack_.empty();
}

bool CommandManager::CanRedo() const {
    return !redo_stack_.empty();
}

void CommandManager::Undo() {
    if (undo_stack_.empty()) {
        return;
    }

    std::unique_ptr<ICommand> command = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    command->Undo();
    redo_stack_.push_back(std::move(command));
}

void CommandManager::Redo() {
    if (redo_stack_.empty()) {
        return;
    }

    std::unique_ptr<ICommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    command->Redo();
    undo_stack_.push_back(std::move(command));
}

void CommandManager::Clear() {
    undo_stack_.clear();
    redo_stack_.clear();
    transaction_commands_.clear();
    in_transaction_ = false;
}

void CommandManager::BeginTransaction(const std::string& description) {
    if (in_transaction_) {
        return;
    }

    in_transaction_ = true;
    transaction_description_ = description;
    transaction_commands_.clear();
}

void CommandManager::EndTransaction() {
    if (!in_transaction_) {
        return;
    }

    in_transaction_ = false;

    if (transaction_commands_.empty()) {
        return;
    }

    undo_stack_.push_back(
        std::make_unique<CompositeCommand>(transaction_description_, std::move(transaction_commands_)));
    redo_stack_.clear();
    transaction_commands_.clear();
}

bool CommandManager::InTransaction() const {
    return in_transaction_;
}

}
