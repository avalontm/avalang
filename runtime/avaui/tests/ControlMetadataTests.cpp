#include "TestFramework.h"

#include "controls/ControlMetadata.h"

using namespace avalang::ui;
using namespace avalang::ui::controls;

AVAUI_TEST(ControlMetadata, ButtonHasClickActivationAndNoStateProperty) {
    const ControlMetadata* metadata = FindControlMetadata("Button");
    EXPECT_TRUE(metadata != nullptr);
    if (!metadata) return;

    EXPECT_TRUE(metadata->stateProperty.empty());
    EXPECT_TRUE(metadata->valueKind == ControlValueKind::None);
    EXPECT_TRUE(metadata->eventName == "click");
    EXPECT_TRUE(metadata->activation == ControlActivation::Click);
}

AVAUI_TEST(ControlMetadata, CheckBoxUsesIsCheckedBoolChange) {
    const ControlMetadata* metadata = FindControlMetadata("CheckBox");
    EXPECT_TRUE(metadata != nullptr);
    if (!metadata) return;

    EXPECT_TRUE(metadata->stateProperty == "isChecked");
    EXPECT_TRUE(metadata->valueKind == ControlValueKind::Bool);
    EXPECT_TRUE(metadata->eventName == "change");
    EXPECT_TRUE(metadata->activation == ControlActivation::Toggle);
}

AVAUI_TEST(ControlMetadata, RadioButtonUsesIsSelectedBoolSelect) {
    const ControlMetadata* metadata = FindControlMetadata("RadioButton");
    EXPECT_TRUE(metadata != nullptr);
    if (!metadata) return;

    EXPECT_TRUE(metadata->stateProperty == "isSelected");
    EXPECT_TRUE(metadata->valueKind == ControlValueKind::Bool);
    EXPECT_TRUE(metadata->activation == ControlActivation::Select);
}

AVAUI_TEST(ControlMetadata, ComboBoxUsesSelectedValueStringPopup) {
    const ControlMetadata* metadata = FindControlMetadata("ComboBox");
    EXPECT_TRUE(metadata != nullptr);
    if (!metadata) return;

    EXPECT_TRUE(metadata->stateProperty == "selectedValue");
    EXPECT_TRUE(metadata->valueKind == ControlValueKind::String);
    EXPECT_TRUE(metadata->activation == ControlActivation::Popup);
}

AVAUI_TEST(ControlMetadata, TextBoxUsesTextStringTextInput) {
    const ControlMetadata* metadata = FindControlMetadata("TextBox");
    EXPECT_TRUE(metadata != nullptr);
    if (!metadata) return;

    EXPECT_TRUE(metadata->stateProperty == "text");
    EXPECT_TRUE(metadata->valueKind == ControlValueKind::String);
    EXPECT_TRUE(metadata->activation == ControlActivation::TextInput);
}

AVAUI_TEST(ControlMetadata, UnknownTypeReturnsNull) {
    EXPECT_TRUE(FindControlMetadata("Slider") == nullptr);
}

AVAUI_TEST(ControlMetadata, TableHasExactlyTheFiveKnownControls) {
    EXPECT_EQ(static_cast<int>(AllControlMetadata().size()), 5);
}
