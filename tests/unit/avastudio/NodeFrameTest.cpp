#include <cmath>
#include <cstdio>

#include "designer/node_frame.h"

namespace {

int g_failures = 0;

bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

void ExpectRect(const char* name, const studio::designer::LayoutRect& actual, double x, double y, double w, double h) {
    if (Near(actual.x, x) && Near(actual.y, y) && Near(actual.width, w) && Near(actual.height, h)) return;
    std::printf("FAIL %s: got {%g, %g, %g, %g} expected {%g, %g, %g, %g}\n", name, actual.x, actual.y, actual.width,
                actual.height, x, y, w, h);
    ++g_failures;
}

void ExpectBool(const char* name, bool actual, bool expected) {
    if (actual == expected) return;
    std::printf("FAIL %s: got %d expected %d\n", name, actual, expected);
    ++g_failures;
}

studio::designer::NodeFrameParams Params(double x, double y, double w, double h) {
    studio::designer::NodeFrameParams params;
    params.layout = {x, y, w, h};
    params.bounds = {0.0, 0.0, 640.0, 480.0};
    return params;
}

}

int main() {
    using namespace studio::designer;

    {
        NodeFrameParams params = Params(10.0, 20.0, 100.0, 30.0);
        params.depth = 1;
        params.skipLeafWireframe = true;
        const NodeFrame frame = ComputeNodeFrame(params);
        ExpectRect("leaf raw", frame.raw, 10.0, 20.0, 100.0, 30.0);
        ExpectRect("leaf selection", frame.selection, 6.0, 16.0, 108.0, 38.0);
        ExpectBool("leaf compact", frame.compact, false);
    }

    {
        NodeFrameParams params = Params(10.0, 20.0, 100.0, 20.0);
        params.depth = 1;
        params.skipLeafWireframe = true;
        const NodeFrame frame = ComputeNodeFrame(params);
        ExpectRect("compact leaf selection", frame.selection, 8.0, 18.0, 104.0, 24.0);
        ExpectBool("compact leaf compact", frame.compact, true);
    }

    {
        NodeFrameParams params = Params(10.0, 20.0, 200.0, 100.0);
        params.isContainer = true;
        const NodeFrame frame = ComputeNodeFrame(params);
        ExpectRect("root content", frame.content, 13.0, 23.0, 194.0, 94.0);
        ExpectRect("root chrome", frame.chrome, 13.0, 23.0, 194.0, 94.0);
        ExpectRect("root selection", frame.selection, 13.0, 23.0, 194.0, 94.0);
        ExpectBool("root compact", frame.compact, false);
    }

    {
        NodeFrameParams params = Params(0.0, 0.0, 100.0, 100.0);
        params.isContainer = true;
        params.depth = 2;
        const NodeFrame frame = ComputeNodeFrame(params);
        ExpectRect("nested content", frame.content, 6.0, 6.0, 88.0, 88.0);
        ExpectRect("nested chrome clamped", frame.chrome, 0.0, 0.0, 110.0, 110.0);
        ExpectRect("nested selection", frame.selection, 0.0, 0.0, 110.0, 110.0);
    }

    {
        NodeFrameParams params = Params(10.0, 20.0, 100.0, 30.0);
        params.offsetY = 20.0;
        params.skipLeafWireframe = true;
        const NodeFrame frame = ComputeNodeFrame(params);
        ExpectRect("offset raw", frame.raw, 10.0, 40.0, 100.0, 30.0);
    }

    {
        const SelectionBox box = ComputeSelectionBox(LayoutRect{0.0, 0.0, 50.0, 10.0}, true);
        ExpectRect("own padding selection", box.rect, 0.0, 0.0, 50.0, 10.0);
        ExpectBool("own padding never compact", box.compact, false);
    }

    if (g_failures == 0) std::printf("NodeFrameTest passed\n");
    return g_failures == 0 ? 0 : 1;
}
