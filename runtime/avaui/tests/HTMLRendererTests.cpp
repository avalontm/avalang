#include "TestFramework.h"

#include "renderer/HTMLRenderer.h"
#include "render_tree/ComboBoxData.h"

using namespace avalang::ui;
using namespace avaui_tests;

namespace {

Color MakeColor(uint8_t r, uint8_t g, uint8_t b) {
    return Color{r, g, b, 255};
}

}

AVAUI_TEST(HTMLRenderer, ButtonEmitsCompIdAndAvaType) {
    HTMLRenderer renderer(400, 300);
    renderer.BeginFrame();
    renderer.DrawButton(10, 10, 100, 30, "Click me", 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(240, 240, 240), MakeColor(150, 150, 150), 1.0f, 0.0f,
        false, "", "", 42, "Button");
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    EXPECT_TRUE(output.find("data-comp-id=\"42\"") != std::string::npos);
    EXPECT_TRUE(output.find("data-ava-type=\"Button\"") != std::string::npos);
}

AVAUI_TEST(HTMLRenderer, CheckBoxEmitsCompIdOnRealInputElement) {
    HTMLRenderer renderer(400, 300);
    renderer.BeginFrame();
    renderer.DrawCheckBox(10, 10, 100, 24, "Accept", 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(255, 255, 255), MakeColor(150, 150, 150), 1.0f, 0.0f,
        true, false, false, false, "", "", 7, "CheckBox");
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    auto inputPos = output.find("<input type=\"checkbox\"");
    auto compIdPos = output.find("data-comp-id=\"7\"");
    EXPECT_TRUE(inputPos != std::string::npos);
    EXPECT_TRUE(compIdPos != std::string::npos);
    EXPECT_TRUE(compIdPos > inputPos);
}

AVAUI_TEST(HTMLRenderer, InputEmitsHtmlInputElement) {
    HTMLRenderer renderer(400, 300);
    renderer.BeginFrame();
    renderer.DrawInput(10, 10, 200, 30, "hello", "placeholder", 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(255, 255, 255), MakeColor(150, 150, 150), 1.0f, 0.0f,
        false, true, false);
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    EXPECT_TRUE(output.find("<input type=\"text\"") != std::string::npos);
    EXPECT_TRUE(output.find("value=\"hello\"") != std::string::npos);
    EXPECT_TRUE(output.find("ava-focused") != std::string::npos);
}

AVAUI_TEST(HTMLRenderer, CheckBoxEmitsHtmlCheckboxInput) {
    HTMLRenderer renderer(400, 300);
    renderer.BeginFrame();
    renderer.DrawCheckBox(10, 10, 100, 24, "Accept", 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(255, 255, 255), MakeColor(150, 150, 150), 1.0f, 0.0f,
        true, false, false, false);
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    EXPECT_TRUE(output.find("type=\"checkbox\"") != std::string::npos);
    EXPECT_TRUE(output.find("checked") != std::string::npos);
    EXPECT_TRUE(output.find("Accept") != std::string::npos);
}

AVAUI_TEST(HTMLRenderer, RadioButtonEmitsHtmlRadioInput) {
    HTMLRenderer renderer(400, 300);
    renderer.BeginFrame();
    renderer.DrawRadioButton(10, 10, 100, 24, "Option A", 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(255, 255, 255), MakeColor(150, 150, 150), 1.0f,
        false, false, false, false);
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    EXPECT_TRUE(output.find("type=\"radio\"") != std::string::npos);
    EXPECT_TRUE(output.find("Option A") != std::string::npos);
}

AVAUI_TEST(HTMLRenderer, ComboBoxEmitsHtmlSelectWithOptions) {
    HTMLRenderer renderer(400, 300);
    std::vector<render::ComboBoxItem> items = {
        {"a", "Alpha", false},
        {"b", "Beta", true},
    };
    renderer.BeginFrame();
    renderer.DrawComboBox(10, 10, 150, 28, items, 12.0f, "Segoe UI",
        MakeColor(0, 0, 0), MakeColor(255, 255, 255), MakeColor(150, 150, 150), 1.0f, 0.0f,
        false, false, false, false);
    renderer.EndFrame();

    std::string output = renderer.GetOutput();
    EXPECT_TRUE(output.find("<select") != std::string::npos);
    EXPECT_TRUE(output.find("<option value=\"a\">Alpha</option>") != std::string::npos);
    EXPECT_TRUE(output.find("<option value=\"b\" selected>Beta</option>") != std::string::npos);
}
