#pragma once

#include <string>

namespace avalang {
namespace ui {
namespace controls {

struct TextEditState {
    std::string text;
    int caret = 0;
    int anchor = 0;
};

int TextEditPrevBoundary(const std::string& text, int pos);
int TextEditNextBoundary(const std::string& text, int pos);
int TextEditClampBoundary(const std::string& text, int pos);

bool TextEditHasSelection(const TextEditState& state);
int TextEditSelectionMin(const TextEditState& state);
int TextEditSelectionMax(const TextEditState& state);
std::string TextEditSelectedText(const TextEditState& state);

TextEditState TextEditInsert(TextEditState state, const std::string& insertion);
TextEditState TextEditDeleteSelection(TextEditState state);
TextEditState TextEditBackspace(TextEditState state);
TextEditState TextEditDeleteForward(TextEditState state);
TextEditState TextEditMoveLeft(TextEditState state, bool extendSelection);
TextEditState TextEditMoveRight(TextEditState state, bool extendSelection);
TextEditState TextEditMoveHome(TextEditState state, bool extendSelection);
TextEditState TextEditMoveEnd(TextEditState state, bool extendSelection);
TextEditState TextEditSelectAll(TextEditState state);

}
}
}
