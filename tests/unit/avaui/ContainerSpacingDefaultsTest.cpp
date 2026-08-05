#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "layout/ILayoutNode.h"
#include "layout/LayoutEngine.h"
#include "layout/LayoutTypes.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using avalang::ui::ComponentTree;
using avalang::ui::IComponent;
using avalang::ui::ILayoutNode;
using avalang::ui::IThemeProvider;
using avalang::ui::LayoutEngine;
using avalang::ui::LayoutRect;
using avalang::ui::PropertyType;
using avalang::ui::PropertyValue;
using avalang::ui::RenderTheme;

namespace {

bool NearlyEqual(double a, double b) {
    return std::fabs(a - b) < 0.001;
}

IComponent* MakeChild(ComponentTree& tree, IComponent* parent, const std::string& type) {
    IComponent* child = tree.CreateComponent(type);
    parent->AddChild(child);
    return child;
}

void TestDefaultsFillWhenAbsent() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Column");
    tree->SetRoot(root);
    MakeChild(*tree, root, "Text");
    MakeChild(*tree, root, "Text");

    std::unique_ptr<IThemeProvider> themeProvider(avalang::ui::CreateDefaultThemeProvider());
    RenderTheme::Apply(tree.get(), themeProvider->Current());

    const PropertyValue* padding = root->GetProperty("padding");
    const PropertyValue* spacing = root->GetProperty("spacing");
    assert(padding != nullptr && padding->Type() == PropertyType::Number);
    assert(spacing != nullptr && spacing->Type() == PropertyType::Number);
    assert(padding->AsNumber() > 0.0);
    assert(spacing->AsNumber() > 0.0);

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 400.0, 300.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    assert(rootNode != nullptr);
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 2);

    const LayoutRect& first = nodes[0]->Rect();
    const LayoutRect& second = nodes[1]->Rect();

    assert(first.y > 0.0);
    assert(second.y - (first.y + first.height) > 0.0);

    std::printf("ContainerSpacingDefaultsTest::TestDefaultsFillWhenAbsent: OK\n");
}

void TestExplicitValuesWin() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Row");
    root->SetProperty("padding", PropertyValue(0.0));
    root->SetProperty("spacing", PropertyValue(0.0));
    tree->SetRoot(root);
    MakeChild(*tree, root, "Text");
    MakeChild(*tree, root, "Text");

    std::unique_ptr<IThemeProvider> themeProvider(avalang::ui::CreateDefaultThemeProvider());
    RenderTheme::Apply(tree.get(), themeProvider->Current());

    const PropertyValue* padding = root->GetProperty("padding");
    const PropertyValue* spacing = root->GetProperty("spacing");
    assert(padding != nullptr && NearlyEqual(padding->AsNumber(), 0.0));
    assert(spacing != nullptr && NearlyEqual(spacing->AsNumber(), 0.0));

    std::unique_ptr<LayoutEngine> engine = LayoutEngine::Create();
    LayoutRect available{0.0, 0.0, 400.0, 100.0};
    engine->Compute(root, available);

    ILayoutNode* rootNode = engine->Root();
    const std::vector<ILayoutNode*>& nodes = rootNode->Children();
    assert(nodes.size() == 2);

    const LayoutRect& first = nodes[0]->Rect();
    const LayoutRect& second = nodes[1]->Rect();
    assert(NearlyEqual(first.x, 0.0));
    assert(NearlyEqual(second.x, first.x + first.width));

    std::printf("ContainerSpacingDefaultsTest::TestExplicitValuesWin: OK\n");
}

void TestNonContainerLeavesUntouched() {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* root = tree->CreateComponent("Button");
    root->SetProperty("text", PropertyValue(std::string("Click")));
    tree->SetRoot(root);

    std::unique_ptr<IThemeProvider> themeProvider(avalang::ui::CreateDefaultThemeProvider());
    RenderTheme::Apply(tree.get(), themeProvider->Current());

    assert(root->GetProperty("spacing") == nullptr);
    assert(root->GetProperty("padding") == nullptr);

    std::printf("ContainerSpacingDefaultsTest::TestNonContainerLeavesUntouched: OK\n");
}

} // namespace

int main() {
    TestDefaultsFillWhenAbsent();
    TestExplicitValuesWin();
    TestNonContainerLeavesUntouched();
    std::printf("ContainerSpacingDefaultsTest: OK\n");
    return 0;
}
