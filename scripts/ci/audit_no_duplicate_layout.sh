#!/usr/bin/env bash
# Phase A.2 of the avastudio/avaui unification plan.
#
# avaui is the single source of truth for layout calculation, .avaui
# parsing/writing, and geometry drawing. This script fails the build if
# a parallel layout engine, a homegrown JSON parser, or direct geometry
# drawing outside the real renderer reappears in avastudio.
#
# Run from repo root:
#   scripts/ci/audit_no_duplicate_layout.sh

set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root" || exit 1

fail=0

check() {
    local name="$1"
    local matches="$2"
    if [ -n "$matches" ]; then
        echo "AUDIT FAIL: $name"
        echo "$matches"
        echo
        fail=1
    fi
}

# 1. Parallel layout engine: any ComputeLayout/EstimateHeight/
#    LayoutResult that isn't a reference to the real
#    avalang::ui::LayoutEngine. The designer_canvas.h line is allowed:
#    it's a comment describing when the real LayoutEngine runs, not a
#    reimplementation.
check1="$(grep -rn "ComputeLayout\|EstimateHeight\|LayoutResult" runtime/avastudio/src \
    | grep -v "avalang::ui::LayoutEngine\|layout/LayoutEngine.h\|LayoutRect" \
    | grep -v "panels/designer_canvas.h:101:")"
check "parallel layout engine (ComputeLayout/EstimateHeight/LayoutResult)" "$check1"

# 2. Homegrown JSON parser for .avaui: no Json class or JsonValue should
#    exist inside runtime/avastudio/src/design.
check2="$(grep -rln "class.*Json\|JsonValue" runtime/avastudio/src/design)"
check "homegrown JSON parser in avastudio/src/design" "$check2"

# 3. Direct geometry drawing outside the real renderer: only
#    imgui_renderer.cpp may call AddRectFilled/AddText/AddEllipse,
#    because it inherits from avalang::ui::BaseRenderer and only draws
#    what the real IRenderTree/ISceneGraph already computed -- it
#    doesn't decide geometry.
check3="$(grep -rn "AddRectFilled\|AddText(\|AddEllipse" runtime/avastudio/src/design \
    | grep -v "design/imgui_renderer.cpp:")"
check "geometry drawing outside imgui_renderer.cpp" "$check3"

if [ "$fail" -ne 0 ]; then
    echo "See PLAN_UNIFICADO_AVAUI.md, Part A: avaui is the"
    echo "single source of truth for layout/parsing/rendering. If"
    echo "avastudio is missing something, add it to avaui, not avastudio."
    exit 1
fi

echo "audit_no_duplicate_layout: OK"
exit 0
