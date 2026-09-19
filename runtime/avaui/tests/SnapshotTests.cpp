#include "TestFramework.h"
#include "TestSupport.h"
#include "Snapshot.h"

#include "controls/Text.h"
#include "controls/Button.h"

using namespace avalang::ui;
using namespace avaui_tests;

namespace {

std::unique_ptr<TestApp> BuildColumnWithTextAndButton() {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);

    IComponent* text = controls::CreateText(app->tree.get(), "hi");
    SetSize(text, 50, 20);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    SetSize(button, 100, 40);

    root->AddChild(text);
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    return app;
}

}

AVAUI_TEST(Snapshot, RenderTreeMatchesStableRepresentation) {
    auto app = BuildColumnWithTextAndButton();

    std::string snapshot = SnapshotRenderTree(app->renderTree->Root());

    std::string expected =
        "Column id=1 bounds=0,0,400,300\n"
        "  Text id=2 bounds=0,0,50,20\n"
        "  Button id=3 bounds=0,32,100,40\n";

    EXPECT_EQ(snapshot, expected);
}

AVAUI_TEST(Snapshot, RenderTreeSnapshotIsStableAcrossRepeatedCalls) {
    auto app = BuildColumnWithTextAndButton();

    std::string first = SnapshotRenderTree(app->renderTree->Root());
    std::string second = SnapshotRenderTree(app->renderTree->Root());

    EXPECT_EQ(first, second);
}

AVAUI_TEST(Snapshot, RenderTreeSnapshotChangesWhenLayoutChanges) {
    auto app = BuildColumnWithTextAndButton();

    std::string before = SnapshotRenderTree(app->renderTree->Root());

    app->Layout(800, 600);
    app->BuildRenderAndScene();
    std::string after = SnapshotRenderTree(app->renderTree->Root());

    EXPECT_TRUE(before != after);
}

AVAUI_TEST(Snapshot, SceneGraphMatchesStableRepresentation) {
    auto app = BuildColumnWithTextAndButton();

    std::string snapshot = SnapshotSceneGraph(*app->sceneGraph);

    std::string expected =
        "Column id=1 visible=true opacity=1 zOrder=0 x=0 y=0\n"
        "  Text id=2 visible=true opacity=1 zOrder=0 x=0 y=0\n"
        "  Button id=3 visible=true opacity=1 zOrder=0 x=0 y=0\n";

    EXPECT_EQ(snapshot, expected);
}

AVAUI_TEST(Snapshot, RenderCommandsMatchStableRepresentation) {
    auto app = BuildColumnWithTextAndButton();

    std::string snapshot = SnapshotRenderCommands(app->sink().GetCommands());

    std::string expected =
        "DrawText\n"
        "DrawButton\n";

    EXPECT_EQ(snapshot, expected);
}

AVAUI_TEST(Snapshot, RenderCommandsSnapshotChangesWhenTreeShapeChanges) {
    auto app = BuildColumnWithTextAndButton();
    std::string before = SnapshotRenderCommands(app->sink().GetCommands());

    IComponent* extraText = controls::CreateText(app->tree.get(), "extra");
    SetSize(extraText, 30, 15);
    app->tree->Root()->AddChild(extraText);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();
    std::string after = SnapshotRenderCommands(app->sink().GetCommands());

    EXPECT_TRUE(before != after);
    EXPECT_EQ(after, std::string("DrawText\nDrawButton\nDrawText\n"));
}
