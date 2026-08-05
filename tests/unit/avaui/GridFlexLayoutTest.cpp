#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "layout/LayoutTypes.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using avalang::ui::ComponentTree;
using avalang::ui::IComponent;
using avalang::ui::ILayoutNode;
using avalang::ui::LayoutEngine;
using avalang::ui::LayoutRect;
using avalang::ui::PropertyValue;

namespace {

bool NearlyEqual(double a, double b) {
    return std::fabs(a - b) < 0.001;
}

IComponent* MakeChild(ComponentTree& tree, IComponent* parent, const std::string& type) {
    IComponent* child = tree.CreateComponent(type);
    parent->AddChild(child);
    return child;
}

void TestGridUniformCells() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Grid");
    tree->SetRoot(root);
    root->SetProperty("columns", PropertyValue(2.0));
    for (int i = 0; i < 4; ++i) {
        MakeChild(*tree, root, "Text");
    }

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 200.0, 100.0};
    engine->Compute(root, available);

    const std::vector<IComponent*>& children = root->Children();
    assert(children.size() == 4);

    ILayoutNode* rootNode = engine->Root();
    assert(rootNode != nullptr);
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 4);

    const LayoutRect& cell0 = nodes[0]->Rect();
    const LayoutRect& cell1 = nodes[1]->Rect();
    const LayoutRect& cell2 = nodes[2]->Rect();
    const LayoutRect& cell3 = nodes[3]->Rect();

    assert(NearlyEqual(cell0.width, 100.0));
    assert(NearlyEqual(cell0.height, 50.0));
    assert(NearlyEqual(cell0.x, 0.0));
    assert(NearlyEqual(cell0.y, 0.0));

    assert(NearlyEqual(cell1.x, 100.0));
    assert(NearlyEqual(cell1.y, 0.0));

    assert(NearlyEqual(cell2.x, 0.0));
    assert(NearlyEqual(cell2.y, 50.0));

    assert(NearlyEqual(cell3.x, 100.0));
    assert(NearlyEqual(cell3.y, 50.0));

    assert(!(NearlyEqual(cell0.x, cell1.x) && NearlyEqual(cell0.y, cell1.y)));
    assert(!(NearlyEqual(cell0.x, cell2.x) && NearlyEqual(cell0.y, cell2.y)));

    std::printf("GridFlexLayoutTest::TestGridUniformCells: OK\n");
}

void TestGridMoreChildrenThanExplicitCells() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Grid");
    tree->SetRoot(root);
    root->SetProperty("columns", PropertyValue(2.0));
    for (int i = 0; i < 5; ++i) {
        MakeChild(*tree, root, "Text");
    }

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 200.0, 150.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 5);

    const LayoutRect& lastCell = nodes[4]->Rect();
    assert(NearlyEqual(lastCell.x, 0.0));
    assert(NearlyEqual(lastCell.y, 100.0));
    assert(lastCell.width > 0.0);
    assert(lastCell.height > 0.0);

    std::printf("GridFlexLayoutTest::TestGridMoreChildrenThanExplicitCells: OK\n");
}

void TestGridSingleChild() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Grid");
    tree->SetRoot(root);
    root->SetProperty("columns", PropertyValue(3.0));
    MakeChild(*tree, root, "Text");

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 300.0, 90.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 1);

    const LayoutRect& cell = nodes[0]->Rect();
    assert(NearlyEqual(cell.x, 0.0));
    assert(NearlyEqual(cell.y, 0.0));
    assert(NearlyEqual(cell.width, 100.0));
    assert(NearlyEqual(cell.height, 90.0));

    std::printf("GridFlexLayoutTest::TestGridSingleChild: OK\n");
}

void TestFlexHorizontal() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Flex");
    tree->SetRoot(root);
    root->SetProperty("direction", PropertyValue(std::string("horizontal")));
    MakeChild(*tree, root, "Text");
    MakeChild(*tree, root, "Text");

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 200.0, 40.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 2);

    const LayoutRect& first = nodes[0]->Rect();
    const LayoutRect& second = nodes[1]->Rect();

    assert(NearlyEqual(first.y, second.y));
    assert(!NearlyEqual(first.x, second.x));
    assert(NearlyEqual(first.width, 100.0));
    assert(NearlyEqual(second.width, 100.0));
    assert(NearlyEqual(second.x, 100.0));

    std::printf("GridFlexLayoutTest::TestFlexHorizontal: OK\n");
}

void TestFlexVertical() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Flex");
    tree->SetRoot(root);
    root->SetProperty("direction", PropertyValue(std::string("vertical")));
    MakeChild(*tree, root, "Text");
    MakeChild(*tree, root, "Text");

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 60.0, 200.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 2);

    const LayoutRect& first = nodes[0]->Rect();
    const LayoutRect& second = nodes[1]->Rect();

    assert(NearlyEqual(first.x, second.x));
    assert(!NearlyEqual(first.y, second.y));
    assert(NearlyEqual(first.height, 100.0));
    assert(NearlyEqual(second.height, 100.0));
    assert(NearlyEqual(second.y, 100.0));

    std::printf("GridFlexLayoutTest::TestFlexVertical: OK\n");
}

void TestFlexDefaultsToVertical() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Flex");
    tree->SetRoot(root);
    MakeChild(*tree, root, "Text");
    MakeChild(*tree, root, "Text");

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 60.0, 200.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 2);

    const LayoutRect& first = nodes[0]->Rect();
    const LayoutRect& second = nodes[1]->Rect();
    assert(NearlyEqual(first.x, second.x));
    assert(!NearlyEqual(first.y, second.y));

    std::printf("GridFlexLayoutTest::TestFlexDefaultsToVertical: OK\n");
}

} // namespace

int main() {
    TestGridUniformCells();
    TestGridMoreChildrenThanExplicitCells();
    TestGridSingleChild();
    TestFlexHorizontal();
    TestFlexVertical();
    TestFlexDefaultsToVertical();
    std::printf("GridFlexLayoutTest: OK\n");
    return 0;
}
