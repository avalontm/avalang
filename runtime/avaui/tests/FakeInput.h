#pragma once

#include "runtime/avalang/platform/interfaces/services/ui/IMouse.h"
#include "runtime/avalang/platform/interfaces/services/ui/IKeyboard.h"
#include "runtime/avalang/platform/interfaces/services/ui/ITextInput.h"

#include <string>
#include <unordered_set>

namespace avaui_tests {

class FakeMouse final : public ava::platform::ui::IMouse {
public:
    void Position(int& x, int& y) const override {
        x = x_;
        y = y_;
    }

    bool IsButtonDown(ava::platform::ui::MouseButton button) const override {
        if (button == ava::platform::ui::MouseButton::Left) return leftDown_;
        if (button == ava::platform::ui::MouseButton::Right) return rightDown_;
        return middleDown_;
    }

    void MoveTo(int x, int y) {
        x_ = x;
        y_ = y;
    }

    void SetLeftDown(bool down) { leftDown_ = down; }
    void SetRightDown(bool down) { rightDown_ = down; }

private:
    int x_ = -1;
    int y_ = -1;
    bool leftDown_ = false;
    bool rightDown_ = false;
    bool middleDown_ = false;
};

class FakeKeyboard final : public ava::platform::ui::IKeyboard {
public:
    bool IsKeyDown(int keyCode) const override { return down_.count(keyCode) > 0; }

    void PressKey(int keyCode) { down_.insert(keyCode); }
    void ReleaseKey(int keyCode) { down_.erase(keyCode); }
    void ReleaseAll() { down_.clear(); }

private:
    std::unordered_set<int> down_;
};

class FakeTextInput final : public ava::platform::ui::ITextInput {
public:
    std::string ConsumeCommittedText() override {
        std::string result = std::move(pending_);
        pending_.clear();
        return result;
    }

    void QueueText(const std::string& text) { pending_ += text; }

private:
    std::string pending_;
};

}
