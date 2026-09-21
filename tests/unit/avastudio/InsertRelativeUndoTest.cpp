#include <iostream>
#include <string>
#include <unordered_map>

#include "components/IComponent.h"
#include "design/design_document.h"
#include "designer/command.h"
#include "designer/selection_manager.h"
#include "designer/tools.h"

#ifdef AVA_STUDIO_TEST_STUB_PARSER
#include "parser/AvauiPropertyCoercion.h"
namespace avalang::ui::parser {
std::string CanonicalTypeName(const std::string& asWritten) { return asWritten; }
}
#endif

namespace {

using studio::design::DropZone;
using studio::designer::CommandManager;
using studio::designer::InsertTool;
using studio::designer::SelectionManager;

int g_checks = 0;
int g_failures = 0;

void Check(const std::string& name, const std::string& actual, const std::string& expected) {
    ++g_checks;
    if (actual == expected) {
        std::cout << "  PASS  " << name << "\n";
        return;
    }
    ++g_failures;
    std::cout << "  FAIL  " << name << "\n        actual:   " << actual << "\n        expected: " << expected << "\n";
}

class Fixture {
public:
    Fixture() : doc_(studio::design::NewBlankAvauiDocument()) {
        column_ = InsertTool::InsertInto(&commands_, doc_, &selection_, doc_.Root()->NodeId(), "Column");
        for (const char* label : {"a", "b", "d"}) {
            labels_[InsertTool::InsertInto(&commands_, doc_, &selection_, column_, "Button")] = label;
        }
    }

    std::string InsertRelative(const std::string& siblingLabel, DropZone zone, const std::string& label) {
        std::string siblingId;
        for (const auto& [id, name] : labels_) {
            if (name == siblingLabel) siblingId = id;
        }
        const std::string id = InsertTool::InsertRelative(&commands_, doc_, &selection_, column_, siblingId, zone, "Text");
        labels_[id] = label;
        return id;
    }

    std::string Order() const {
        std::string out;
        const avalang::ui::IComponent* column = studio::design::FindNodeById(doc_.Root(), column_);
        for (const avalang::ui::IComponent* child : column->Children()) {
            const auto it = labels_.find(child->NodeId());
            out += it != labels_.end() ? it->second : "?";
        }
        return out;
    }

    CommandManager& Commands() { return commands_; }
    studio::design::DesignDocument& Doc() { return doc_; }
    SelectionManager& Selection() { return selection_; }
    const std::string& ColumnId() const { return column_; }
    std::unordered_map<std::string, std::string>& Labels() { return labels_; }

private:
    studio::design::DesignDocument doc_;
    CommandManager commands_;
    SelectionManager selection_;
    std::string column_;
    std::unordered_map<std::string, std::string> labels_;
};

void RunSingleUndoStepCases() {
    {
        Fixture f;
        f.InsertRelative("b", DropZone::kBefore, "x");
        Check("insert before middle", f.Order(), "axbd");
        f.Commands().Undo();
        Check("one undo removes the inserted node", f.Order(), "abd");
        f.Commands().Redo();
        Check("redo restores the inserted node in place", f.Order(), "axbd");
        f.Commands().Undo();
        Check("undo after redo removes it again", f.Order(), "abd");
    }
    {
        Fixture f;
        f.InsertRelative("d", DropZone::kAfter, "x");
        Check("insert after last", f.Order(), "abdx");
        f.Commands().Undo();
        Check("undo insert after last", f.Order(), "abd");
    }
    {
        Fixture f;
        f.InsertRelative("a", DropZone::kBefore, "x");
        Check("insert before first", f.Order(), "xabd");
        f.Commands().Undo();
        Check("undo insert before first", f.Order(), "abd");
    }
    {
        Fixture f;
        f.InsertRelative("b", DropZone::kBefore, "x");
        f.InsertRelative("d", DropZone::kBefore, "y");
        Check("two positional inserts", f.Order(), "axbyd");
        f.Commands().Undo();
        Check("first undo removes the last insert only", f.Order(), "axbd");
        f.Commands().Undo();
        Check("second undo removes the earlier insert", f.Order(), "abd");
    }
}

void RunTransactionCases() {
    {
        Fixture f;
        f.InsertRelative("b", DropZone::kBefore, "x");
        Check("transaction is closed after insert", f.Commands().InTransaction() ? "open" : "closed", "closed");
    }
    {
        Fixture f;
        f.Commands().BeginTransaction("outer");
        f.InsertRelative("b", DropZone::kBefore, "x");
        Check("outer transaction stays open", f.Commands().InTransaction() ? "open" : "closed", "open");
        f.Commands().EndTransaction();
        f.Commands().Undo();
        Check("outer transaction undoes as one step", f.Order(), "abd");
    }
}

void RunNoManagerCases() {
    studio::design::DesignDocument doc = studio::design::NewBlankAvauiDocument();
    const std::string column = InsertTool::InsertInto(nullptr, doc, nullptr, doc.Root()->NodeId(), "Column");
    const std::string first = InsertTool::InsertInto(nullptr, doc, nullptr, column, "Button");
    const std::string second = InsertTool::InsertInto(nullptr, doc, nullptr, column, "Button");
    const std::string inserted = InsertTool::InsertRelative(nullptr, doc, nullptr, column, second, DropZone::kBefore, "Text");

    std::string order;
    for (const avalang::ui::IComponent* child : studio::design::FindNodeById(doc.Root(), column)->Children()) {
        order += child->NodeId() == first ? "1" : child->NodeId() == second ? "2" : child->NodeId() == inserted ? "x" : "?";
    }
    Check("insert without command manager", order, "1x2");
}

}

int main() {
    const std::pair<const char*, void (*)()> suites[] = {
        {"InsertRelative undo steps", RunSingleUndoStepCases},
        {"InsertRelative transactions", RunTransactionCases},
        {"InsertRelative without manager", RunNoManagerCases},
    };
    for (const auto& [title, fn] : suites) {
        std::cout << title << "\n";
        fn();
    }
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
