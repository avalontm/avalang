#pragma once

#include <memory>
#include <string>

#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "layout/LayoutEngine.h"
#include "layout/LayoutTypes.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "renderer/IRenderer.h"
#include "theme/ITheme.h"
#include "theme/RenderTheme.h"
#include "events/EventDispatcher.h"

#include "FakeInput.h"

namespace avaui_tests {

inline void SetSize(avalang::ui::IComponent* component, double width, double height) {
    component->SetProperty("width", avalang::ui::PropertyValue(width));
    component->SetProperty("height", avalang::ui::PropertyValue(height));
}

struct TestApp {
    std::unique_ptr<avalang::ui::ComponentTree> tree;
    std::unique_ptr<avalang::ui::IThemeProvider> themeProvider;
    std::unique_ptr<avalang::ui::LayoutEngine> layoutEngine;
    std::unique_ptr<avalang::ui::render::IRenderTree> renderTree;
    std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph;
    std::unique_ptr<avalang::ui::IRenderer> renderer;
    std::unique_ptr<avalang::ui::RenderCommandSink> sinkPtr;

    avalang::ui::RenderCommandSink& sink() { return *sinkPtr; }
    const avalang::ui::RenderCommandSink& sink() const { return *sinkPtr; }

    avalang::ui::IComponent* Root() const { return tree->Root(); }

    void ApplyTheme() {
        avalang::ui::RenderTheme::Apply(tree.get(), themeProvider->Current());
    }

    void Layout(double width, double height) {
        avalang::ui::LayoutRect viewport{0.0, 0.0, width, height};
        layoutEngine->Compute(tree->Root(), viewport);
    }

    void BuildRenderAndScene() {
        renderTree->Build(tree->Root(), layoutEngine.get());
        sceneGraph->Build(renderTree->Root());
        sceneGraph->UpdateTransforms();
    }

    void WalkCommands() {
        sinkPtr = std::make_unique<avalang::ui::RenderCommandSink>();
        avalang::ui::SceneCommandWalker::Walk(*sceneGraph, *sinkPtr, *renderer);
    }
};

inline std::unique_ptr<TestApp> MakeTestApp(double width = 800.0, double height = 600.0) {
    auto app = std::make_unique<TestApp>();
    app->tree = avalang::ui::ComponentTree::Create();
    app->themeProvider.reset(avalang::ui::CreateDefaultThemeProvider());
    app->layoutEngine = avalang::ui::LayoutEngine::Create();
    app->renderTree.reset(avalang::ui::render::IRenderTree::Create());
    app->sceneGraph.reset(avalang::ui::scene::ISceneGraph::Create());
    app->renderer = avalang::ui::IRenderer::Create("html", static_cast<int>(width), static_cast<int>(height));
    app->sinkPtr = std::make_unique<avalang::ui::RenderCommandSink>();
    return app;
}

class RecordingHandler final : public avalang::ui::events::IEventHandler {
public:
    void OnEvent(avalang::ui::events::IEvent* event) override { ++count_; }

    int Count() const { return count_; }

private:
    int count_ = 0;
};

class EventCapture final : public avalang::ui::events::IEventHandler {
public:
    void OnEvent(avalang::ui::events::IEvent* event) override {
        ++count_;
        type_ = event->Type();
        target_ = event->Target();

        if (auto* pointer = dynamic_cast<avalang::ui::events::IPointerEvent*>(event)) {
            button_ = pointer->Button();
            x_ = pointer->X();
            y_ = pointer->Y();
        }
        if (auto* keyboard = dynamic_cast<avalang::ui::events::IKeyboardEvent*>(event)) {
            keyCode_ = keyboard->KeyCode();
            translatedKey_ = keyboard->TranslatedKey();
        }
        if (auto* textInput = dynamic_cast<avalang::ui::events::ITextInputEvent*>(event)) {
            text_ = textInput->Text();
        }
        if (auto* focus = dynamic_cast<avalang::ui::events::IFocusEvent*>(event)) {
            relatedTarget_ = focus->RelatedTarget();
        }
    }

    int Count() const { return count_; }
    avalang::ui::events::EventType Type() const { return type_; }
    avalang::ui::ComponentId Target() const { return target_; }
    avalang::ui::events::PointerButton Button() const { return button_; }
    int X() const { return x_; }
    int Y() const { return y_; }
    int KeyCode() const { return keyCode_; }
    avalang::ui::events::Key TranslatedKey() const { return translatedKey_; }
    const std::string& Text() const { return text_; }
    avalang::ui::ComponentId RelatedTarget() const { return relatedTarget_; }

private:
    int count_ = 0;
    avalang::ui::events::EventType type_ = avalang::ui::events::EventType::Custom;
    avalang::ui::ComponentId target_ = 0;
    avalang::ui::events::PointerButton button_ = avalang::ui::events::PointerButton::None;
    int x_ = 0;
    int y_ = 0;
    int keyCode_ = 0;
    avalang::ui::events::Key translatedKey_ = avalang::ui::events::Key::Unknown;
    std::string text_;
    avalang::ui::ComponentId relatedTarget_ = 0;
};

struct InputHarness {
    avalang::ui::events::EventDispatcher dispatcher;
    FakeMouse mouse;
    FakeKeyboard keyboard;
    FakeTextInput textInput;

    void Attach(avalang::ui::LayoutEngine* layoutEngine) {
        dispatcher.SetLayoutEngine(layoutEngine);
        dispatcher.SetPlatformInput(&mouse, &keyboard);
        dispatcher.SetTextInputSource(&textInput);
    }

    void ClickAt(avalang::ui::IComponent* root, int x, int y) {
        mouse.MoveTo(x, y);
        dispatcher.PollInput(root);
        mouse.SetLeftDown(true);
        dispatcher.PollInput(root);
        mouse.SetLeftDown(false);
        dispatcher.PollInput(root);
    }

    void PressKey(avalang::ui::IComponent* root, int keyCode) {
        keyboard.PressKey(keyCode);
        dispatcher.PollInput(root);
    }

    void ReleaseKey(avalang::ui::IComponent* root, int keyCode) {
        keyboard.ReleaseKey(keyCode);
        dispatcher.PollInput(root);
    }

    void TypeText(avalang::ui::IComponent* root, const std::string& text) {
        textInput.QueueText(text);
        dispatcher.PollInput(root);
    }
};

}
