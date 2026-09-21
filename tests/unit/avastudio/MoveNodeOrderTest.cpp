#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "design/design_document.h"
#include "designer/move_commands.h"

namespace {

using avalang::ui::ComponentTree;
using avalang::ui::IComponent;
using avalang::ui::PropertyValue;
using studio::design::DropZone;

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

std::vector<std::string> Split(const std::string& csv) {
    std::vector<std::string> out;
    std::string current;
    for (char c : csv) {
        if (c == ',') {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

class Scene {
public:
    Scene() : tree_(ComponentTree::Create()) {
        root_ = tree_->CreateComponent("Page");
        tree_->SetRoot(root_);
        main_ = AddNode(root_, "main", "Column");
        other_ = AddNode(root_, "other", "Column");
    }

    IComponent* AddNode(IComponent* parent, const std::string& label, const std::string& type = "Button") {
        IComponent* node = tree_->CreateComponent(type);
        node->SetProperty("id", PropertyValue(label));
        parent->AddChild(node);
        ids_[label] = node->NodeId();
        nodes_[label] = node;
        return node;
    }

    IComponent* AddNodeToSlot(IComponent* parent, const std::string& slot, const std::string& label) {
        IComponent* node = tree_->CreateComponent("Button");
        node->SetProperty("id", PropertyValue(label));
        parent->AddChild(node, slot);
        ids_[label] = node->NodeId();
        nodes_[label] = node;
        return node;
    }

    std::string SlotOrder(IComponent* parent, const std::string& slot) const {
        std::string out;
        for (IComponent* child : parent->SlotChildren(slot)) {
            if (!out.empty()) out.push_back(',');
            out += child->GetProperty("id")->AsString();
        }
        return out;
    }

    void Populate(IComponent* parent, const std::string& csv) {
        for (const std::string& label : Split(csv)) AddNode(parent, label);
    }

    bool Move(const std::string& moved, const std::string& target, DropZone zone) {
        return studio::design::MoveNode(root_, ids_.at(moved), ids_.at(target), zone);
    }

    std::string Order(IComponent* parent) const {
        std::string out;
        for (IComponent* child : parent->Children()) {
            if (!out.empty()) out.push_back(',');
            out += child->GetProperty("id")->AsString();
        }
        return out;
    }

    bool ParentsConsistent(IComponent* parent) const {
        for (IComponent* child : parent->Children()) {
            if (child->Parent() != parent) return false;
        }
        return true;
    }

    ComponentTree* Tree() const { return tree_.get(); }
    IComponent* Root() const { return root_; }
    IComponent* Main() const { return main_; }
    IComponent* Other() const { return other_; }
    IComponent* Node(const std::string& label) const { return nodes_.at(label); }
    const std::string& Id(const std::string& label) const { return ids_.at(label); }

private:
    std::unique_ptr<ComponentTree> tree_;
    IComponent* root_ = nullptr;
    IComponent* main_ = nullptr;
    IComponent* other_ = nullptr;
    std::unordered_map<std::string, std::string> ids_;
    std::unordered_map<std::string, IComponent*> nodes_;
};

struct OrderCase {
    std::string name;
    std::string initial;
    std::string moved;
    std::string target;
    DropZone zone;
    std::string expected;
};

void RunOrderCase(const OrderCase& c, bool movedIsExternal) {
    Scene scene;
    scene.Populate(scene.Main(), c.initial);
    if (movedIsExternal) {
        scene.Populate(scene.Other(), "Y," + c.moved + ",Z");
    }
    const bool ok = scene.Move(c.moved, c.target, c.zone);
    const std::string prefix = movedIsExternal ? "external " : "same-parent ";
    Check(prefix + c.name + " returns true", ok ? "true" : "false", "true");
    Check(prefix + c.name + " order", scene.Order(scene.Main()), c.expected);
    if (movedIsExternal) {
        Check(prefix + c.name + " source parent", scene.Order(scene.Other()), "Y,Z");
    }
    Check(prefix + c.name + " parent links",
          scene.ParentsConsistent(scene.Main()) && scene.ParentsConsistent(scene.Other()) ? "ok" : "broken", "ok");
}

void RunExternalOrderCases() {
    const std::vector<OrderCase> cases = {
        {"before middle", "A,B,C,D,E", "X", "C", DropZone::kBefore, "A,B,X,C,D,E"},
        {"after middle", "A,B,C,D,E", "X", "C", DropZone::kAfter, "A,B,C,X,D,E"},
        {"before first", "A,B,C,D,E", "X", "A", DropZone::kBefore, "X,A,B,C,D,E"},
        {"after first", "A,B,C,D,E", "X", "A", DropZone::kAfter, "A,X,B,C,D,E"},
        {"before last", "A,B,C,D,E", "X", "E", DropZone::kBefore, "A,B,C,D,X,E"},
        {"after last", "A,B,C,D,E", "X", "E", DropZone::kAfter, "A,B,C,D,E,X"},
        {"before only child", "A", "X", "A", DropZone::kBefore, "X,A"},
        {"after only child", "A", "X", "A", DropZone::kAfter, "A,X"},
    };
    for (const OrderCase& c : cases) RunOrderCase(c, true);
}

void RunSameParentOrderCases() {
    const std::vector<OrderCase> cases = {
        {"last before second", "A,B,C,D,E", "E", "B", DropZone::kBefore, "A,E,B,C,D"},
        {"first after third", "A,B,C,D,E", "A", "C", DropZone::kAfter, "B,C,A,D,E"},
        {"forward before", "A,B,C,D,E", "B", "D", DropZone::kBefore, "A,C,B,D,E"},
        {"forward after", "A,B,C,D,E", "B", "D", DropZone::kAfter, "A,C,D,B,E"},
        {"backward before", "A,B,C,D,E", "D", "B", DropZone::kBefore, "A,D,B,C,E"},
        {"backward after", "A,B,C,D,E", "D", "B", DropZone::kAfter, "A,B,D,C,E"},
        {"first before last", "A,B,C,D,E", "A", "E", DropZone::kBefore, "B,C,D,A,E"},
        {"last after first", "A,B,C,D,E", "E", "A", DropZone::kAfter, "A,E,B,C,D"},
        {"noop before neighbour", "A,B,C,D,E", "A", "B", DropZone::kBefore, "A,B,C,D,E"},
        {"noop after neighbour", "A,B,C,D,E", "B", "A", DropZone::kAfter, "A,B,C,D,E"},
        {"noop last after previous", "A,B,C,D,E", "E", "D", DropZone::kAfter, "A,B,C,D,E"},
        {"two children swap", "A,B", "A", "B", DropZone::kAfter, "B,A"},
    };
    for (const OrderCase& c : cases) RunOrderCase(c, false);
}

void RunIntoCases() {
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        scene.Populate(scene.Other(), "X,Y");
        const bool ok = scene.Move("X", "main", DropZone::kInto);
        Check("into appends external node", scene.Order(scene.Main()), "A,B,C,X");
        Check("into external source", scene.Order(scene.Other()), "Y");
        Check("into returns true", ok ? "true" : "false", "true");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        scene.Move("A", "main", DropZone::kInto);
        Check("into same parent moves to end", scene.Order(scene.Main()), "B,C,A");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        scene.Move("B", "other", DropZone::kInto);
        Check("into empty container", scene.Order(scene.Other()), "B");
        Check("into empty container source", scene.Order(scene.Main()), "A,C");
    }
}

void RunGuardCases() {
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        const bool ok = scene.Move("main", "A", DropZone::kBefore);
        Check("guard container into own descendant returns false", ok ? "true" : "false", "false");
        Check("guard container into own descendant keeps tree", scene.Order(scene.Root()), "main,other");
        Check("guard container into own descendant keeps children", scene.Order(scene.Main()), "A,B,C");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        const bool ok = scene.Move("B", "B", DropZone::kBefore);
        Check("guard self target returns false", ok ? "true" : "false", "false");
        Check("guard self target keeps order", scene.Order(scene.Main()), "A,B,C");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        const bool ok = studio::design::MoveNode(scene.Root(), scene.Root()->NodeId(), scene.Id("A"), DropZone::kBefore);
        Check("guard root move returns false", ok ? "true" : "false", "false");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        const bool ok = studio::design::MoveNode(scene.Root(), scene.Id("A"), "missing", DropZone::kBefore);
        Check("guard unknown target returns false", ok ? "true" : "false", "false");
        Check("guard unknown target keeps order", scene.Order(scene.Main()), "A,B,C");
    }
}

void RunRootTargetGuardCases() {
    Scene scene;
    scene.Populate(scene.Main(), "A,B,C");
    const bool before = studio::design::MoveNode(scene.Root(), scene.Id("B"), scene.Root()->NodeId(), DropZone::kBefore);
    Check("guard root as before target returns false", before ? "true" : "false", "false");
    Check("guard root as before target keeps order", scene.Order(scene.Main()), "A,B,C");
    const bool after = studio::design::MoveNode(scene.Root(), scene.Id("B"), scene.Root()->NodeId(), DropZone::kAfter);
    Check("guard root as after target returns false", after ? "true" : "false", "false");
    Check("guard root as after target keeps order", scene.Order(scene.Main()), "A,B,C");
}

void RunSlotCases() {
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C");
        scene.AddNodeToSlot(scene.Main(), "extra", "S1");
        scene.AddNodeToSlot(scene.Main(), "extra", "S2");
        scene.Move("C", "A", DropZone::kBefore);
        Check("slots default reordered", scene.SlotOrder(scene.Main(), "default"), "C,A,B");
        Check("slots extra untouched", scene.SlotOrder(scene.Main(), "extra"), "S1,S2");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B");
        scene.AddNodeToSlot(scene.Main(), "extra", "S1");
        scene.AddNodeToSlot(scene.Main(), "extra", "S2");
        scene.Move("A", "S2", DropZone::kBefore);
        Check("slots target slot receives node", scene.SlotOrder(scene.Main(), "extra"), "S1,A,S2");
        Check("slots source slot loses node", scene.SlotOrder(scene.Main(), "default"), "B");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B");
        scene.AddNodeToSlot(scene.Main(), "extra", "S1");
        scene.AddNodeToSlot(scene.Main(), "extra", "S2");
        scene.Move("S1", "S2", DropZone::kAfter);
        Check("slots reorder inside extra", scene.SlotOrder(scene.Main(), "extra"), "S2,S1");
        Check("slots default untouched", scene.SlotOrder(scene.Main(), "default"), "A,B");
    }
}

void RunContainerSiblingCases() {
    Scene scene;
    scene.AddNode(scene.Root(), "third", "Column");
    Check("container siblings initial", scene.Order(scene.Root()), "main,other,third");
    scene.Move("third", "main", DropZone::kBefore);
    Check("container reorder before", scene.Order(scene.Root()), "third,main,other");
    scene.Move("third", "other", DropZone::kAfter);
    Check("container reorder after", scene.Order(scene.Root()), "main,other,third");
}

void RunUndoRedoCases() {
    using studio::designer::MoveComponentCommand;
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C,D,E");
        MoveComponentCommand cmd(scene.Tree(), scene.Id("B"), scene.Id("D"), DropZone::kBefore);
        cmd.Execute();
        Check("undo same parent execute", scene.Order(scene.Main()), "A,C,B,D,E");
        cmd.Undo();
        Check("undo same parent undo", scene.Order(scene.Main()), "A,B,C,D,E");
        cmd.Redo();
        Check("undo same parent redo", scene.Order(scene.Main()), "A,C,B,D,E");
        cmd.Undo();
        Check("undo same parent undo again", scene.Order(scene.Main()), "A,B,C,D,E");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C,D,E");
        scene.Populate(scene.Other(), "Y,X,Z");
        MoveComponentCommand cmd(scene.Tree(), scene.Id("X"), scene.Id("C"), DropZone::kBefore);
        cmd.Execute();
        Check("undo external execute main", scene.Order(scene.Main()), "A,B,X,C,D,E");
        Check("undo external execute other", scene.Order(scene.Other()), "Y,Z");
        cmd.Undo();
        Check("undo external undo main", scene.Order(scene.Main()), "A,B,C,D,E");
        Check("undo external undo other", scene.Order(scene.Other()), "Y,X,Z");
        cmd.Redo();
        Check("undo external redo main", scene.Order(scene.Main()), "A,B,X,C,D,E");
        Check("undo external redo other", scene.Order(scene.Other()), "Y,Z");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C,D,E");
        scene.Populate(scene.Other(), "Y,Z");
        MoveComponentCommand cmd(scene.Tree(), scene.Id("E"), scene.Id("Y"), DropZone::kBefore);
        cmd.Execute();
        Check("undo last child execute other", scene.Order(scene.Other()), "E,Y,Z");
        cmd.Undo();
        Check("undo last child undo main", scene.Order(scene.Main()), "A,B,C,D,E");
        Check("undo last child undo other", scene.Order(scene.Other()), "Y,Z");
    }
    {
        Scene scene;
        scene.Populate(scene.Main(), "A,B,C,D,E");
        scene.Populate(scene.Other(), "Y,Z");
        MoveComponentCommand cmd(scene.Tree(), scene.Id("A"), scene.Id("Z"), DropZone::kAfter);
        cmd.Execute();
        Check("undo first child execute other", scene.Order(scene.Other()), "Y,Z,A");
        cmd.Undo();
        Check("undo first child undo main", scene.Order(scene.Main()), "A,B,C,D,E");
        Check("undo first child undo other", scene.Order(scene.Other()), "Y,Z");
    }
}

}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> suites = {
        {"MoveNode order: external node", RunExternalOrderCases},
        {"MoveNode order: same parent", RunSameParentOrderCases},
        {"MoveNode into", RunIntoCases},
        {"MoveNode guards", RunGuardCases},
        {"MoveNode root target guards", RunRootTargetGuardCases},
        {"MoveNode slots", RunSlotCases},
        {"MoveNode container siblings", RunContainerSiblingCases},
        {"MoveComponentCommand undo/redo", RunUndoRedoCases},
    };
    for (const auto& [title, fn] : suites) {
        std::cout << title << "\n";
        fn();
    }
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
