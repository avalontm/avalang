#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "designer/command.h"
#include "designer/document_commands.h"
#include "designer/selection_manager.h"
#include "parser/AvauiWriter.h"

namespace {

using studio::design::DesignDocument;
using studio::designer::CommandManager;
using studio::designer::SelectionManager;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    ++g_failures;
}

std::string Snapshot(const DesignDocument& doc) {
    avalang::ui::parser::AvauiWriteOptions options;
    return avalang::ui::parser::WriteAvaui(doc.Root(), options);
}

struct Step {
    std::string name;
    std::function<bool()> run;
};

}

int main() {
    DesignDocument doc = studio::design::NewBlankAvauiDocument();
    CommandManager commands;
    SelectionManager selection;

    const std::string column = studio::designer::ExecuteAddComponent(&commands, doc, &selection, "", "Column", "col", {});
    const std::string b1 = studio::designer::ExecuteAddComponent(&commands, doc, &selection, column, "Button", "b1", {});
    const std::string b2 = studio::designer::ExecuteAddComponent(&commands, doc, &selection, column, "Button", "b2", {});
    const std::string b3 = studio::designer::ExecuteAddComponent(&commands, doc, &selection, column, "Button", "b3", {});
    Check(!column.empty() && !b1.empty() && !b2.empty() && !b3.empty(), "setup creates nodes");

    std::vector<std::string> duplicated(1);

    const std::vector<Step> steps = {
        {"delete middle sibling",
         [&] { return studio::designer::ExecuteRemoveComponent(&commands, doc, &selection, b2); }},
        {"duplicate first sibling",
         [&] {
             duplicated[0] = studio::designer::ExecuteDuplicateComponent(&commands, doc, &selection, b1);
             return !duplicated[0].empty();
         }},
        {"set property",
         [&] { return studio::designer::ExecuteSetProperty(&commands, doc, &selection, b3, "width", "120"); }},
        {"set event",
         [&] { return studio::designer::ExecuteSetProperty(&commands, doc, &selection, b3, "click", "OnSave"); }},
        {"remove property",
         [&] { return studio::designer::ExecuteRemoveProperty(&commands, doc, &selection, b3, "width"); }},
        {"move before first",
         [&] {
             return studio::designer::ExecuteMoveComponent(&commands, doc, &selection, b3, b1,
                                                            studio::design::DropZone::kBefore);
         }},
        {"reparent into root",
         [&] {
             return studio::designer::ExecuteMoveComponent(&commands, doc, &selection, b1, doc.Root()->NodeId(),
                                                            studio::design::DropZone::kInto);
         }},
        {"delete first child",
         [&] { return studio::designer::ExecuteRemoveComponent(&commands, doc, &selection, b3); }},
    };

    std::vector<std::string> snapshots;
    snapshots.push_back(Snapshot(doc));
    for (const Step& step : steps) {
        Check(step.run(), "step runs: " + step.name);
        snapshots.push_back(Snapshot(doc));
        Check(snapshots.back() != snapshots[snapshots.size() - 2], "step changes document: " + step.name);
    }

    for (size_t i = steps.size(); i > 0; --i) {
        Check(commands.CanUndo(), "can undo: " + steps[i - 1].name);
        commands.Undo();
        Check(Snapshot(doc) == snapshots[i - 1], "undo restores document: " + steps[i - 1].name);
    }

    for (size_t i = 0; i < steps.size(); ++i) {
        Check(commands.CanRedo(), "can redo: " + steps[i].name);
        commands.Redo();
        Check(Snapshot(doc) == snapshots[i + 1], "redo restores document: " + steps[i].name);
    }

    for (size_t i = steps.size(); i > 0; --i) {
        commands.Undo();
    }

    Check(!studio::design::IsPropertyAuthored(doc, b3, "width"), "authored mark cleared after undo");
    commands.Redo();
    commands.Redo();
    commands.Redo();
    Check(studio::design::IsPropertyAuthored(doc, b3, "width"), "authored mark restored after redo");

    if (g_failures == 0) {
        std::printf("DesignerHistoryTest: OK\n");
        return 0;
    }
    return 1;
}
