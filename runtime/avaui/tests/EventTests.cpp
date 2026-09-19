#include "TestFramework.h"
#include "TestSupport.h"

#include "controls/Button.h"
#include "controls/TextBox.h"
#include "controls/Container.h"

using namespace avalang::ui;
using namespace avaui_tests;

AVAUI_TEST(Event, PointerDownFiresOnPress) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::PointerDown, &capture);

    input.mouse.MoveTo(5, 5);
    input.dispatcher.PollInput(root);
    input.mouse.SetLeftDown(true);
    input.dispatcher.PollInput(root);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
    EXPECT_TRUE(capture.Button() == events::PointerButton::Left);
    EXPECT_EQ(capture.X(), 5);
    EXPECT_EQ(capture.Y(), 5);
}

AVAUI_TEST(Event, PointerUpFiresOnRelease) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::PointerUp, &capture);

    input.mouse.MoveTo(5, 5);
    input.dispatcher.PollInput(root);
    input.mouse.SetLeftDown(true);
    input.dispatcher.PollInput(root);
    input.mouse.SetLeftDown(false);
    input.dispatcher.PollInput(root);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
    EXPECT_TRUE(capture.Button() == events::PointerButton::Left);
}

AVAUI_TEST(Event, ClickFiresOnceOnPressAndReleaseOverSameTarget) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &capture);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
}

AVAUI_TEST(Event, FocusFiresWhenTargetBecomesFocused) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Focus, &capture);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
    EXPECT_EQ(capture.RelatedTarget(), static_cast<ComponentId>(0));
    EXPECT_EQ(input.dispatcher.FocusedComponent(), button->Id());
}

AVAUI_TEST(Event, BlurFiresWhenFocusMovesToAnotherTarget) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* first = controls::CreateButton(app->tree.get(), "first");
    IComponent* second = controls::CreateButton(app->tree.get(), "second");
    SetSize(first, 100, 50);
    SetSize(second, 100, 50);
    root->AddChild(first);
    root->AddChild(second);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    EventCapture capture;
    input.dispatcher.Subscribe(first->Id(), events::EventType::Blur, &capture);

    input.ClickAt(root, 10, 10);
    input.ClickAt(root, 10, 70);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), first->Id());
    EXPECT_EQ(capture.RelatedTarget(), second->Id());
}

AVAUI_TEST(Event, KeyDownFiresOnFocusedComponent) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    input.dispatcher.SetKeyTranslator([](int keyCode) {
        return keyCode == 65 ? events::Key::A : events::Key::Unknown;
    });

    input.ClickAt(root, 5, 5);

    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::KeyDown, &capture);

    input.PressKey(root, 65);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
    EXPECT_EQ(capture.KeyCode(), 65);
    EXPECT_TRUE(capture.TranslatedKey() == events::Key::A);
}

AVAUI_TEST(Event, TextInputFiresOnFocusedTextBox) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* textBox = controls::CreateTextBox(app->tree.get(), "placeholder");
    root->AddChild(textBox);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());

    input.ClickAt(root, 5, 5);

    EventCapture capture;
    input.dispatcher.Subscribe(textBox->Id(), events::EventType::TextInput, &capture);

    input.TypeText(root, "hi");

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), textBox->Id());
    EXPECT_EQ(capture.Text(), std::string("hi"));
}
