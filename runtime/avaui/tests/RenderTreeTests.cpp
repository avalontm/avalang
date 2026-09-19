#include "TestFramework.h"
#include "TestSupport.h"

#include "controls/Text.h"
#include "controls/Button.h"
#include "controls/CheckBox.h"
#include "controls/Container.h"

using namespace avalang::ui;
using namespace avaui_tests;

AVAUI_TEST(RenderTree, RootTypeMatchesContainerKind) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    root->AddChild(controls::CreateText(app->tree.get(), "hi"));

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    EXPECT_EQ(app->renderTree->Root()->Type(), render::RenderNodeType::Column);
}

AVAUI_TEST(RenderTree, BoundsAreSetFromLayout) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    auto node = app->renderTree->FindNode(button->Id());
    EXPECT_TRUE(node != nullptr);
    LayoutRect rect = node->Rect();
    EXPECT_TRUE(rect.width > 0.0);
    EXPECT_TRUE(rect.height > 0.0);
}

AVAUI_TEST(RenderTree, ComponentIdRoundTripsThroughFindNode) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    auto node = app->renderTree->FindNode(checkBox->Id());
    EXPECT_TRUE(node != nullptr);
    EXPECT_EQ(node->Id(), checkBox->Id());
}

AVAUI_TEST(RenderTree, DisabledStatePropagatesToRenderNode) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    button->SetProperty("disabled", PropertyValue(true));
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    auto node = app->renderTree->FindNode(button->Id());
    EXPECT_TRUE(node != nullptr);
    EXPECT_TRUE(node->Disabled());
}

AVAUI_TEST(RenderTree, RootChildrenCountMatchesDirectChildren) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    root->AddChild(controls::CreateText(app->tree.get(), "a"));
    root->AddChild(controls::CreateButton(app->tree.get(), "b"));
    root->AddChild(controls::CreateCheckBox(app->tree.get(), "c", false));

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    EXPECT_EQ(app->renderTree->Root()->Children().size(), static_cast<size_t>(3));
}

AVAUI_TEST(RenderTree, NestedContainerOwnsOnlyItsOwnChildren) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);

    IComponent* row = controls::CreateRow(app->tree.get());
    IComponent* rowButton = controls::CreateButton(app->tree.get(), "row button");
    IComponent* rowCheck = controls::CreateCheckBox(app->tree.get(), "row check", false);
    row->AddChild(rowButton);
    row->AddChild(rowCheck);
    root->AddChild(row);

    IComponent* siblingText = controls::CreateText(app->tree.get(), "sibling");
    root->AddChild(siblingText);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    EXPECT_EQ(app->renderTree->Root()->Children().size(), static_cast<size_t>(2));

    auto rowNode = app->renderTree->FindNode(row->Id());
    EXPECT_TRUE(rowNode != nullptr);
    EXPECT_EQ(rowNode->Children().size(), static_cast<size_t>(2));

    bool hasRowButton = false;
    bool hasRowCheck = false;
    bool hasSiblingLeaked = false;
    for (const auto& child : rowNode->Children()) {
        if (child->Id() == rowButton->Id()) hasRowButton = true;
        if (child->Id() == rowCheck->Id()) hasRowCheck = true;
        if (child->Id() == siblingText->Id()) hasSiblingLeaked = true;
    }
    EXPECT_TRUE(hasRowButton);
    EXPECT_TRUE(hasRowCheck);
    EXPECT_FALSE(hasSiblingLeaked);
}

AVAUI_TEST(RenderTree, NestedChildBoundsStayInsideParentBounds) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);

    IComponent* row = controls::CreateRow(app->tree.get());
    IComponent* rowButton = controls::CreateButton(app->tree.get(), "row button");
    row->AddChild(rowButton);
    root->AddChild(row);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();

    auto rowNode = app->renderTree->FindNode(row->Id());
    auto buttonNode = app->renderTree->FindNode(rowButton->Id());
    EXPECT_TRUE(rowNode != nullptr);
    EXPECT_TRUE(buttonNode != nullptr);

    LayoutRect rowRect = rowNode->Rect();
    LayoutRect buttonRect = buttonNode->Rect();
    EXPECT_TRUE(buttonRect.x >= rowRect.x);
    EXPECT_TRUE(buttonRect.y >= rowRect.y);
    EXPECT_TRUE(buttonRect.x + buttonRect.width <= rowRect.x + rowRect.width + 0.01);
    EXPECT_TRUE(buttonRect.y + buttonRect.height <= rowRect.y + rowRect.height + 0.01);
}
