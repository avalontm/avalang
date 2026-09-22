#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "design/injected_properties.h"
#include "designer/responsive_inspector.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"

namespace {

namespace fs = std::filesystem;
using namespace avalang::ui;

int g_failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << "\n";
    ++g_failures;
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

struct Project {
    fs::path root;

    Project() {
        root = fs::temp_directory_path() / "ava_responsive_inspector_test";
        fs::remove_all(root);
        fs::create_directories(root);
        std::ofstream(root / "app.ava") << "style \"styles.ava\"\n";
        std::ofstream(root / "styles.ava") << "style Text\n"
                                             "    fontSize = 14\n"
                                             "    textColor = 111111\n"
                                             "end\n"
                                             "\n"
                                             "style Text@800\n"
                                             "    fontSize = 22\n"
                                             "end\n"
                                             "\n"
                                             "style *@1200\n"
                                             "    padding = 32\n"
                                             "end\n";
    }

    ~Project() { fs::remove_all(root); }
};

struct Scene {
    std::unique_ptr<ComponentTree> tree = ComponentTree::Create();
    IComponent* page = nullptr;
    IComponent* text = nullptr;

    Scene() {
        page = tree->CreateComponent("Page");
        tree->SetRoot(page);
        text = tree->CreateComponent("Text");
        text->SetProperty("text", PropertyValue(std::string("hello")));
        page->AddChild(text);
    }
};

double FontSizeOf(IComponent* node) {
    const PropertyValue* value = node->GetProperty("fontSize");
    return value != nullptr ? value->AsNumber() : -1.0;
}

void Render(Scene& scene, const theme::ProjectStyleSheet& styles, double width) {
    std::unique_ptr<IThemeProvider> provider(CreateDefaultThemeProvider());
    RenderTheme::Apply(scene.tree.get(), provider->Current(), &styles, width);
}

void InjectedValuesGoStaleWithoutRevert() {
    Project project;
    const theme::ProjectStyleSheet styles = theme::LoadProjectStyleOverrides(project.root.string());
    Scene scene;
    Render(scene, styles, 1000.0);
    CHECK(FontSizeOf(scene.text) == 22.0);
    Render(scene, styles, 400.0);
    CHECK(FontSizeOf(scene.text) == 22.0);
}

void RevertLetsBreakpointsReResolve() {
    Project project;
    const theme::ProjectStyleSheet styles = theme::LoadProjectStyleOverrides(project.root.string());
    Scene scene;
    const studio::design::PropertySnapshot before = studio::design::SnapshotProperties(scene.tree.get());
    Render(scene, styles, 1000.0);
    const std::vector<studio::design::InjectedProperty> injected =
        studio::design::DiffInjectedProperties(before, scene.tree.get());
    CHECK(FontSizeOf(scene.text) == 22.0);

    bool sawFontSize = false;
    bool sawAuthoredText = false;
    for (const studio::design::InjectedProperty& property : injected) {
        if (property.key == "fontSize") sawFontSize = true;
        if (property.key == "text") sawAuthoredText = true;
    }
    CHECK(sawFontSize);
    CHECK(!sawAuthoredText);

    const size_t reverted = studio::design::RevertInjectedProperties(scene.tree.get(), injected, nullptr);
    CHECK(reverted == injected.size());
    CHECK(scene.text->GetProperty("fontSize") == nullptr);
    CHECK(scene.text->GetProperty("text") != nullptr);

    Render(scene, styles, 400.0);
    CHECK(FontSizeOf(scene.text) == 14.0);
}

void RevertSkipsAuthoredProperties() {
    Project project;
    const theme::ProjectStyleSheet styles = theme::LoadProjectStyleOverrides(project.root.string());
    Scene scene;
    const studio::design::PropertySnapshot before = studio::design::SnapshotProperties(scene.tree.get());
    Render(scene, styles, 1000.0);
    const std::vector<studio::design::InjectedProperty> injected =
        studio::design::DiffInjectedProperties(before, scene.tree.get());

    const std::string textId = scene.text->NodeId();
    const studio::design::AuthoredPredicate authored = [&](const std::string& nodeId, const std::string& key) {
        return nodeId == textId && key == "fontSize";
    };
    studio::design::RevertInjectedProperties(scene.tree.get(), injected, authored);
    CHECK(FontSizeOf(scene.text) == 22.0);
    CHECK(scene.text->GetProperty("textColor") == nullptr);
}

void RevertIgnoresMissingNodes() {
    Scene scene;
    const std::vector<studio::design::InjectedProperty> injected{{"uid_missing", "fontSize"}};
    CHECK(studio::design::RevertInjectedProperties(scene.tree.get(), injected, nullptr) == 0);
}

void InspectorReportsActiveBreakpoints() {
    Project project;
    const theme::ProjectStyleSheet styles = theme::LoadProjectStyleOverrides(project.root.string());
    using namespace studio::designer;

    CHECK(!HighestActiveBreakpoint(styles, 400.0).has_value());
    CHECK(HighestActiveBreakpoint(styles, 800.0) == 800u);
    CHECK(HighestActiveBreakpoint(styles, 1000.0) == 800u);
    CHECK(HighestActiveBreakpoint(styles, 1200.0) == 1200u);
}

void InspectorRowsCarryBreakpointSource() {
    Project project;
    const theme::ProjectStyleSheet styles = theme::LoadProjectStyleOverrides(project.root.string());
    using namespace studio::designer;

    const auto find = [](const std::vector<ResponsiveStyleRow>& rows, const std::string& key) {
        for (const ResponsiveStyleRow& row : rows) {
            if (row.key == key) return &row;
        }
        return static_cast<const ResponsiveStyleRow*>(nullptr);
    };

    const std::vector<ResponsiveStyleRow> narrow = ResolveResponsiveRows(styles, "text", 400.0);
    CHECK(find(narrow, "fontSize") != nullptr && find(narrow, "fontSize")->value == "14");
    CHECK(!find(narrow, "fontSize")->breakpointMinWidth.has_value());
    CHECK(find(narrow, "padding") == nullptr);

    const std::vector<ResponsiveStyleRow> wide = ResolveResponsiveRows(styles, "text", 1300.0);
    CHECK(find(wide, "fontSize") != nullptr && find(wide, "fontSize")->value == "22");
    CHECK(find(wide, "fontSize")->breakpointMinWidth == 800u);
    CHECK(find(wide, "textColor") != nullptr && !find(wide, "textColor")->breakpointMinWidth.has_value());
    CHECK(find(wide, "padding") != nullptr && find(wide, "padding")->value == "32");
    CHECK(find(wide, "padding")->breakpointMinWidth == 1200u);
}

}

int main() {
    InjectedValuesGoStaleWithoutRevert();
    RevertLetsBreakpointsReResolve();
    RevertSkipsAuthoredProperties();
    RevertIgnoresMissingNodes();
    InspectorReportsActiveBreakpoints();
    InspectorRowsCarryBreakpointSource();
    if (g_failures == 0) std::cout << "all passed\n";
    return g_failures == 0 ? 0 : 1;
}
