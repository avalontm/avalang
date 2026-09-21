#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/avastudio_dragdrop_tests"
mkdir -p "$OUT"

FLAGS=(-std=c++20 -ffunction-sections -Wl,--gc-sections -Iruntime/avaui/src -Iruntime/avastudio/src)

TREE=(
    runtime/avaui/src/components/Component.cpp
    runtime/avaui/src/components/ComponentTreeImpl.cpp
    runtime/avaui/src/components/ComponentTree.cpp
    runtime/avaui/src/components/PropertyValue.cpp
)

STUDIO=runtime/avastudio/src
TESTS=tests/unit/avastudio

build_and_run() {
    local name="$1"
    shift
    echo "== $name"
    "$CXX" "${FLAGS[@]}" "$@" -o "$OUT/$name"
    if ! "$OUT/$name" > "$OUT/$name.log"; then
        grep -v "PASS" "$OUT/$name.log"
        return 1
    fi
    tail -1 "$OUT/$name.log"
}

build_and_run move_node_order_test \
    "$TESTS/MoveNodeOrderTest.cpp" \
    "$STUDIO/design/design_document.cpp" \
    "$STUDIO/designer/move_commands.cpp" \
    "${TREE[@]}"

build_and_run drop_zone_test \
    "$TESTS/DropZoneTest.cpp" \
    "$STUDIO/designer/drop_target.cpp"

build_and_run insert_relative_undo_test \
    -DAVA_STUDIO_TEST_STUB_PARSER \
    "$TESTS/InsertRelativeUndoTest.cpp" \
    "$STUDIO/design/design_document.cpp" \
    "$STUDIO/designer/tools.cpp" \
    "$STUDIO/designer/command.cpp" \
    "$STUDIO/designer/document_commands.cpp" \
    "$STUDIO/designer/move_commands.cpp" \
    "$STUDIO/designer/lifecycle_commands.cpp" \
    "$STUDIO/designer/selection_manager.cpp" \
    "${TREE[@]}"
