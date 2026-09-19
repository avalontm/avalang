#include "TestFramework.h"
#include "TestSupport.h"

#include "controls/Text.h"
#include "controls/Button.h"
#include "controls/CheckBox.h"
#include "controls/RadioButton.h"
#include "controls/Container.h"

using namespace avalang::ui;
using namespace avaui_tests;

AVAUI_TEST(HitTest, ClickOnLabelHitsTheLabel) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* label = controls::CreateText(app->tree.get(), "hello");
    SetSize(label, 100, 30);
    root->AddChild(label);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler handler;
    input.dispatcher.Subscribe(label->Id(), events::EventType::PointerDown, &handler);

    input.ClickAt(root, 10, 10);

    EXPECT_TRUE(handler.Count() >= 1);
}

AVAUI_TEST(HitTest, ClickOnCheckBoxHitsTheCheckBox) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler handler;
    input.dispatcher.Subscribe(checkBox->Id(), events::EventType::Click, &handler);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(handler.Count(), 1);
}

AVAUI_TEST(HitTest, ClickOnRadioButtonHitsTheRadioButton) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* radio = controls::CreateRadioButton(app->tree.get(), "option a", "group1", false);
    root->AddChild(radio);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler handler;
    input.dispatcher.Subscribe(radio->Id(), events::EventType::Click, &handler);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(handler.Count(), 1);
}

AVAUI_TEST(HitTest, ClickOnButtonHitsTheButton) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler handler;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &handler);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(handler.Count(), 1);
}

AVAUI_TEST(HitTest, ClickOutsideAnyComponentHitsNothing) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler buttonHandler;
    RecordingHandler rootHandler;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &buttonHandler);
    input.dispatcher.Subscribe(root->Id(), events::EventType::Click, &rootHandler);

    input.ClickAt(root, 5000, 5000);

    EXPECT_EQ(buttonHandler.Count(), 0);
    EXPECT_EQ(rootHandler.Count(), 0);
}

AVAUI_TEST(HitTest, DisabledTargetReceivesPointerButNotClick) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    button->SetProperty("isEnabled", PropertyValue(false));
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler pointerDown;
    RecordingHandler click;
    input.dispatcher.Subscribe(button->Id(), events::EventType::PointerDown, &pointerDown);
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &click);

    input.ClickAt(root, 5, 5);

    EXPECT_TRUE(pointerDown.Count() >= 1);
    EXPECT_EQ(click.Count(), 0);
}

AVAUI_TEST(HitTest, OverlappingTargetsResolveToTheFirstInteractiveOwner) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Stack");
    app->tree->SetRoot(root);
    IComponent* front = controls::CreateButton(app->tree.get(), "front");
    IComponent* back = controls::CreateButton(app->tree.get(), "back");
    SetSize(front, 200, 100);
    SetSize(back, 200, 100);
    root->AddChild(front);
    root->AddChild(back);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    RecordingHandler frontHandler;
    RecordingHandler backHandler;
    input.dispatcher.Subscribe(front->Id(), events::EventType::PointerDown, &frontHandler);
    input.dispatcher.Subscribe(back->Id(), events::EventType::PointerDown, &backHandler);

    input.ClickAt(root, 10, 10);

    EXPECT_TRUE(frontHandler.Count() >= 1);
    EXPECT_EQ(backHandler.Count(), 0);
}
