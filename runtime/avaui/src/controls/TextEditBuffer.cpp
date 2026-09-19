#include "controls/TextEditBuffer.h"

namespace avalang {
namespace ui {
namespace controls {

namespace {

bool IsContinuationByte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

}

int TextEditPrevBoundary(const std::string& text, int pos) {
    const int size = static_cast<int>(text.size());
    if (pos > size) pos = size;
    if (pos <= 0) return 0;

    int p = pos - 1;
    while (p > 0 && IsContinuationByte(text[p])) {
        --p;
    }
    return p;
}

int TextEditNextBoundary(const std::string& text, int pos) {
    const int size = static_cast<int>(text.size());
    if (pos < 0) pos = 0;
    if (pos >= size) return size;

    int p = pos + 1;
    while (p < size && IsContinuationByte(text[p])) {
        ++p;
    }
    return p;
}

int TextEditClampBoundary(const std::string& text, int pos) {
    const int size = static_cast<int>(text.size());
    if (pos < 0) pos = 0;
    if (pos > size) pos = size;
    while (pos > 0 && pos < size && IsContinuationByte(text[pos])) {
        --pos;
    }
    return pos;
}

bool TextEditHasSelection(const TextEditState& state) {
    return state.caret != state.anchor;
}

int TextEditSelectionMin(const TextEditState& state) {
    return state.caret < state.anchor ? state.caret : state.anchor;
}

int TextEditSelectionMax(const TextEditState& state) {
    return state.caret > state.anchor ? state.caret : state.anchor;
}

std::string TextEditSelectedText(const TextEditState& state) {
    if (!TextEditHasSelection(state)) return std::string();
    const int lo = TextEditSelectionMin(state);
    const int hi = TextEditSelectionMax(state);
    return state.text.substr(lo, hi - lo);
}

TextEditState TextEditDeleteSelection(TextEditState state) {
    if (!TextEditHasSelection(state)) return state;
    const int lo = TextEditSelectionMin(state);
    const int hi = TextEditSelectionMax(state);
    state.text.erase(lo, hi - lo);
    state.caret = lo;
    state.anchor = lo;
    return state;
}

TextEditState TextEditInsert(TextEditState state, const std::string& insertion) {
    if (insertion.empty()) return state;
    if (TextEditHasSelection(state)) {
        state = TextEditDeleteSelection(state);
    }

    const int pos = TextEditClampBoundary(state.text, state.caret);
    state.text.insert(static_cast<size_t>(pos), insertion);
    state.caret = pos + static_cast<int>(insertion.size());
    state.anchor = state.caret;
    return state;
}

TextEditState TextEditBackspace(TextEditState state) {
    if (TextEditHasSelection(state)) {
        return TextEditDeleteSelection(state);
    }

    const int pos = TextEditClampBoundary(state.text, state.caret);
    const int prev = TextEditPrevBoundary(state.text, pos);
    if (prev == pos) return state;

    state.text.erase(static_cast<size_t>(prev), static_cast<size_t>(pos - prev));
    state.caret = prev;
    state.anchor = prev;
    return state;
}

TextEditState TextEditDeleteForward(TextEditState state) {
    if (TextEditHasSelection(state)) {
        return TextEditDeleteSelection(state);
    }

    const int pos = TextEditClampBoundary(state.text, state.caret);
    const int next = TextEditNextBoundary(state.text, pos);
    if (next == pos) return state;

    state.text.erase(static_cast<size_t>(pos), static_cast<size_t>(next - pos));
    state.caret = pos;
    state.anchor = pos;
    return state;
}

TextEditState TextEditMoveLeft(TextEditState state, bool extendSelection) {
    if (!extendSelection && TextEditHasSelection(state)) {
        state.caret = TextEditSelectionMin(state);
        state.anchor = state.caret;
        return state;
    }

    const int pos = TextEditClampBoundary(state.text, state.caret);
    state.caret = TextEditPrevBoundary(state.text, pos);
    if (!extendSelection) state.anchor = state.caret;
    return state;
}

TextEditState TextEditMoveRight(TextEditState state, bool extendSelection) {
    if (!extendSelection && TextEditHasSelection(state)) {
        state.caret = TextEditSelectionMax(state);
        state.anchor = state.caret;
        return state;
    }

    const int pos = TextEditClampBoundary(state.text, state.caret);
    state.caret = TextEditNextBoundary(state.text, pos);
    if (!extendSelection) state.anchor = state.caret;
    return state;
}

TextEditState TextEditMoveHome(TextEditState state, bool extendSelection) {
    state.caret = 0;
    if (!extendSelection) state.anchor = 0;
    return state;
}

TextEditState TextEditMoveEnd(TextEditState state, bool extendSelection) {
    state.caret = static_cast<int>(state.text.size());
    if (!extendSelection) state.anchor = state.caret;
    return state;
}

TextEditState TextEditSelectAll(TextEditState state) {
    state.anchor = 0;
    state.caret = static_cast<int>(state.text.size());
    return state;
}

}
}
}
