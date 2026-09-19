#include "TestFramework.h"
#include "TestSupport.h"

#include "controls/Button.h"
#include "controls/ButtonController.h"
#include "controls/CheckBox.h"
#include "controls/CheckBoxController.h"
#include "controls/RadioButton.h"
#include "controls/RadioButtonController.h"

using namespace avalang::ui;
using namespace avaui_tests;

namespace {

constexpr int kTabKeyCode = 9;
constexpr int kSpaceKeyCode = 32;
constexpr int kEnterKeyCode = 13;

void UseTabAndSpaceTranslator(events::EventDispatcher& dispatcher) {
    dispatcher.SetKeyTranslator([](int keyCode) {
        switch (keyCode) {
            case kTabKeyCode: return events::Key::Tab;
            case kSpaceKeyCode: return events::Key::Space;
            case kEnterKeyCode: return events::Key::Enter;
            default: return events::Key::Unknown;
        }
    });
}

}

AVAUI_TEST(ControlActivation, SpaceOnFocusedButtonFiresClick) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    controls::ButtonController buttonController(input.dispatcher);
    buttonController.Attach(root);

    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &capture);

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
    EXPECT_TRUE(capture.Type() == events::EventType::Click);
}

AVAUI_TEST(ControlActivation, EnterOnFocusedButtonFiresClick) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    controls::ButtonController buttonController(input.dispatcher);
    buttonController.Attach(root);

    EventCapture capture;
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &capture);

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kEnterKeyCode);

    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), button->Id());
}

AVAUI_TEST(ControlActivation, SpaceOnFocusedCheckBoxTogglesIt) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    controls::CheckBoxController checkBoxController(input.dispatcher);
    checkBoxController.Attach(root);

    EventCapture capture;
    input.dispatcher.Subscribe(checkBox->Id(), events::EventType::Change, &capture);

    EXPECT_FALSE(controls::GetCheckBoxChecked(checkBox));

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    EXPECT_TRUE(controls::GetCheckBoxChecked(checkBox));
    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), checkBox->Id());

    input.ReleaseKey(root, kSpaceKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    EXPECT_FALSE(controls::GetCheckBoxChecked(checkBox));
    EXPECT_EQ(capture.Count(), 2);
}

AVAUI_TEST(ControlActivation, BoundCheckBoxTogglesFromDisplayedStateAndKeepsBinding) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    checkBox->SetProperty("isChecked", PropertyValue(std::string("agreed")));
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    bool agreed = true;
    controls::CheckBoxBinding binding;
    binding.resolveChecked = [&agreed](IComponent*) { return agreed; };
    binding.commitChecked = [&agreed](IComponent*, bool next) {
        agreed = next;
        return true;
    };

    controls::CheckBoxController checkBoxController(input.dispatcher);
    checkBoxController.Attach(root);
    checkBoxController.SetBinding(binding);

    EventCapture capture;
    input.dispatcher.Subscribe(checkBox->Id(), events::EventType::Change, &capture);

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    // The first activation must flip what the user sees (true -> false), write
    // it back to the variable, and leave `isChecked = agreed` as a binding.
    EXPECT_FALSE(agreed);
    EXPECT_EQ(capture.Count(), 1);
    const PropertyValue* prop = checkBox->GetProperty("isChecked");
    EXPECT_TRUE(prop != nullptr);
    EXPECT_TRUE(prop->Type() == PropertyType::String);
    EXPECT_TRUE(prop->AsString() == "agreed");

    input.ReleaseKey(root, kSpaceKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    EXPECT_TRUE(agreed);
    EXPECT_EQ(capture.Count(), 2);
}

AVAUI_TEST(ControlActivation, CheckBoxWithoutBindingHooksStillOverwritesWithLiteral) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);
    root->AddChild(checkBox);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    // A hook that declines to handle the toggle must fall back to the classic
    // behavior (literal bool written to the component).
    controls::CheckBoxBinding binding;
    binding.commitChecked = [](IComponent*, bool) { return false; };

    controls::CheckBoxController checkBoxController(input.dispatcher);
    checkBoxController.Attach(root);
    checkBoxController.SetBinding(binding);

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kSpaceKeyCode);

    EXPECT_TRUE(controls::GetCheckBoxChecked(checkBox));
}

AVAUI_TEST(ControlActivation, GetCheckBoxCheckedReadsTextLiteralButNotBindings) {
    auto app = MakeTestApp();
    IComponent* checkBox = controls::CreateCheckBox(app->tree.get(), "agree", false);

    checkBox->SetProperty("isChecked", PropertyValue(std::string("true")));
    EXPECT_TRUE(controls::GetCheckBoxChecked(checkBox));

    checkBox->SetProperty("isChecked", PropertyValue(std::string("agreed")));
    EXPECT_FALSE(controls::GetCheckBoxChecked(checkBox));
}

AVAUI_TEST(ControlActivation, ButtonControllerOwnsPointerAndFocusEventsWithoutRegression) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* button = controls::CreateButton(app->tree.get(), "go");
    root->AddChild(button);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());

    controls::ButtonController buttonController(input.dispatcher);
    buttonController.Attach(root);

    EventCapture pointerDown, pointerUp, click, focus;
    input.dispatcher.Subscribe(button->Id(), events::EventType::PointerDown, &pointerDown);
    input.dispatcher.Subscribe(button->Id(), events::EventType::PointerUp, &pointerUp);
    input.dispatcher.Subscribe(button->Id(), events::EventType::Click, &click);
    input.dispatcher.Subscribe(button->Id(), events::EventType::Focus, &focus);

    input.ClickAt(root, 5, 5);

    EXPECT_EQ(pointerDown.Count(), 1);
    EXPECT_EQ(pointerUp.Count(), 1);
    EXPECT_EQ(click.Count(), 1);
    EXPECT_EQ(focus.Count(), 1);
}

AVAUI_TEST(ControlActivation, EnterOnFocusedRadioButtonSelectsIt) {
    auto app = MakeTestApp();
    IComponent* root = app->tree->CreateComponent("Column");
    app->tree->SetRoot(root);
    IComponent* radio = controls::CreateRadioButton(app->tree.get(), "yes", "group1", false);
    root->AddChild(radio);

    app->ApplyTheme();
    app->Layout(400, 300);

    InputHarness input;
    input.Attach(app->layoutEngine.get());
    UseTabAndSpaceTranslator(input.dispatcher);

    controls::RadioButtonController radioButtonController(input.dispatcher);
    radioButtonController.Attach(root);

    EventCapture capture;
    input.dispatcher.Subscribe(radio->Id(), events::EventType::Change, &capture);

    EXPECT_FALSE(controls::GetRadioButtonSelected(radio));

    input.PressKey(root, kTabKeyCode);
    input.PressKey(root, kEnterKeyCode);

    EXPECT_TRUE(controls::GetRadioButtonSelected(radio));
    EXPECT_EQ(capture.Count(), 1);
    EXPECT_EQ(capture.Target(), radio->Id());

    input.ReleaseKey(root, kEnterKeyCode);
    input.PressKey(root, kEnterKeyCode);

    EXPECT_TRUE(controls::GetRadioButtonSelected(radio));
    EXPECT_EQ(capture.Count(), 1);
}
