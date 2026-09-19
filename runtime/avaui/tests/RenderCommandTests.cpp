#include "TestFramework.h"
#include "TestSupport.h"

#include "controls/Text.h"
#include "controls/Button.h"
#include "controls/TextBox.h"
#include "controls/CheckBox.h"
#include "controls/RadioButton.h"
#include "controls/ComboBox.h"
#include "controls/Container.h"

using namespace avalang::ui;
using namespace avaui_tests;

namespace {

int CountCommandsOfType(const TestApp& app, RenderCommandType type) {
    int count = 0;
    for (const auto& cmd : app.sink().GetCommands()) {
        if (cmd.type == type) ++count;
    }
    return count;
}

}

AVAUI_TEST(RenderCommand, TextEmitsDrawText) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* text = controls::CreateText(app->tree.get(), "hello");
    root->AddChild(text);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_TRUE(CountCommandsOfType(*app, RenderCommandType::DrawText) >= 1);
}

AVAUI_TEST(RenderCommand, ButtonEmitsDrawButton) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "Click me");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawButton), 1);
}

AVAUI_TEST(RenderCommand, TextBoxEmitsDrawInput) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* textBox = controls::CreateTextBox(app->tree.get(), "placeholder");
    root->AddChild(textBox);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawInput), 1);
}

AVAUI_TEST(RenderCommand, CheckBoxEmitsDrawCheckBox) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawCheckBox), 1);
}

AVAUI_TEST(RenderCommand, RadioButtonEmitsDrawRadioButton) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* radio = controls::CreateRadioButton(app->tree.get(), "option a", "group1", false);
    root->AddChild(radio);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawRadioButton), 1);
}

AVAUI_TEST(RenderCommand, ComboBoxEmitsDrawComboBox) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* combo = controls::CreateComboBox(app->tree.get());
    controls::AddOption(app->tree.get(), combo, "a", "A");
    controls::AddOption(app->tree.get(), combo, "b", "B");
    root->AddChild(combo);

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawComboBox), 1);
}

AVAUI_TEST(RenderCommand, MixedTreeEmitsOneCommandPerControl) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    root->AddChild(controls::CreateText(app->tree.get(), "label"));
    root->AddChild(controls::CreateButton(app->tree.get(), "go"));
    root->AddChild(controls::CreateCheckBox(app->tree.get(), "check", true));

    app->ApplyTheme();
    app->Layout(400, 300);
    app->BuildRenderAndScene();
    app->WalkCommands();

    EXPECT_TRUE(CountCommandsOfType(*app, RenderCommandType::DrawText) >= 1);
    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawButton), 1);
    EXPECT_EQ(CountCommandsOfType(*app, RenderCommandType::DrawCheckBox), 1);
}
