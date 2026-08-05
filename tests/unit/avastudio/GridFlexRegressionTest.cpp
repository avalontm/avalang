#include "design/design_document.h"
#include "design/live_render_bridge.h"
#include "layout/LayoutTypes.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using avalang::ui::LayoutRect;
using studio::design::BuildLiveRender;
using studio::design::DesignDocument;
using studio::design::LiveRenderResult;
using studio::design::ParseAvauiText;

namespace {

bool NearlyEqual(double a, double b) {
    return std::fabs(a - b) < 0.001;
}

bool SameCell(const LayoutRect& a, const LayoutRect& b) {
    return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y);
}

const char* kSource = R"(view
    column
        grid
            id = "TheGrid"
            columns = 2

            text
                id = "Cell0"
            end
            text
                id = "Cell1"
            end
            text
                id = "Cell2"
            end
            text
                id = "Cell3"
            end
        end

        flex
            id = "TheFlex"
            direction = "horizontal"

            text
                id = "FlexA"
            end
            text
                id = "FlexB"
            end
        end
    end
end
)";

} // namespace

int main() {
    DesignDocument doc;
    std::string error;
    bool parsed = ParseAvauiText(kSource, doc, error);
    assert(parsed);
    assert(doc.tree && doc.tree->Root());

    LiveRenderResult result = BuildLiveRender(doc.tree.get(), 400, 300);
    assert(result.ok);
    assert(result.error.empty());

    const LayoutRect& cell0 = result.nodeIdToRect.at("Cell0");
    const LayoutRect& cell1 = result.nodeIdToRect.at("Cell1");
    const LayoutRect& cell2 = result.nodeIdToRect.at("Cell2");
    const LayoutRect& cell3 = result.nodeIdToRect.at("Cell3");

    // Antes de A.3, Grid caia al fallback de ArrangeStack y las cuatro
    // celdas se superponian sobre el mismo rect (overlay). Con el
    // algoritmo real, cada celda debe caer en una posicion distinta.
    assert(!SameCell(cell0, cell1));
    assert(!SameCell(cell0, cell2));
    assert(!SameCell(cell0, cell3));
    assert(!SameCell(cell1, cell3));
    assert(NearlyEqual(cell0.y, cell1.y));
    assert(NearlyEqual(cell0.x, cell2.x));
    assert(cell0.width > 0.0);
    assert(cell0.height > 0.0);

    const LayoutRect& flexA = result.nodeIdToRect.at("FlexA");
    const LayoutRect& flexB = result.nodeIdToRect.at("FlexB");
    assert(!SameCell(flexA, flexB));
    assert(NearlyEqual(flexA.y, flexB.y));
    assert(!NearlyEqual(flexA.x, flexB.x));

    std::printf("GridFlexRegressionTest: OK\n");
    return 0;
}
