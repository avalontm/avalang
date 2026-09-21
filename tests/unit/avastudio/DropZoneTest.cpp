#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "designer/drop_target.h"

namespace {

using studio::design::DropZone;
using studio::designer::ComputeDropIndicator;
using studio::designer::ComputeDropZone;
using studio::designer::ComputeInsertIndex;
using studio::designer::ComputeInsertMarker;
using studio::designer::ResolveInsertPosition;
using studio::designer::FlowAxis;
using studio::designer::FlowLayout;
using studio::designer::FlowLayoutOf;
using studio::designer::LayoutPoint;
using studio::designer::LayoutRect;

int g_checks = 0;
int g_failures = 0;

std::string Name(DropZone zone) {
    switch (zone) {
        case DropZone::kBefore: return "before";
        case DropZone::kInto: return "into";
        case DropZone::kAfter: return "after";
    }
    return "unknown";
}

void CheckZone(const std::string& name, DropZone actual, DropZone expected) {
    ++g_checks;
    if (actual == expected) {
        std::cout << "  PASS  " << name << "\n";
        return;
    }
    ++g_failures;
    std::cout << "  FAIL  " << name << "\n        actual:   " << Name(actual) << "\n        expected: " << Name(expected)
              << "\n";
}

void CheckBool(const std::string& name, bool actual, bool expected) {
    ++g_checks;
    if (actual == expected) {
        std::cout << "  PASS  " << name << "\n";
        return;
    }
    ++g_failures;
    std::cout << "  FAIL  " << name << "\n";
}

void CheckIndex(const std::string& name, size_t actual, size_t expected) {
    ++g_checks;
    if (actual == expected) {
        std::cout << "  PASS  " << name << "\n";
        return;
    }
    ++g_failures;
    std::cout << "  FAIL  " << name << "\n        actual:   " << actual << "\n        expected: " << expected << "\n";
}

bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

bool MarkerIs(const FlowLayout& flow, const std::vector<LayoutRect>& rects, size_t index, double x, double y, double w,
              double h) {
    LayoutRect marker;
    if (!ComputeInsertMarker(flow, rects, index, marker)) return false;
    return Near(marker.x, x) && Near(marker.y, y) && Near(marker.width, w) && Near(marker.height, h);
}

DropZone ContainerZone(double height, double y, bool allowSibling = true) {
    return ComputeDropZone(LayoutRect{0.0, 0.0, 200.0, height}, LayoutPoint{10.0, y}, true, allowSibling);
}

DropZone LeafZone(double height, double y) {
    return ComputeDropZone(LayoutRect{0.0, 0.0, 200.0, height}, LayoutPoint{10.0, y}, false, true);
}

void RunTallContainerCases() {
    CheckZone("tall top edge is before", ContainerZone(300.0, 0.0), DropZone::kBefore);
    CheckZone("tall inside top band is before", ContainerZone(300.0, 15.0), DropZone::kBefore);
    CheckZone("tall band is capped at 16px", ContainerZone(300.0, 17.0), DropZone::kInto);
    CheckZone("tall center is into", ContainerZone(300.0, 150.0), DropZone::kInto);
    CheckZone("tall just above bottom band is into", ContainerZone(300.0, 283.0), DropZone::kInto);
    CheckZone("tall inside bottom band is after", ContainerZone(300.0, 285.0), DropZone::kAfter);
    CheckZone("tall bottom edge is after", ContainerZone(300.0, 300.0), DropZone::kAfter);
}

void RunMinimumContainerCases() {
    CheckZone("56px top is before", ContainerZone(56.0, 4.0), DropZone::kBefore);
    CheckZone("56px past top band is into", ContainerZone(56.0, 12.0), DropZone::kInto);
    CheckZone("56px middle is into", ContainerZone(56.0, 28.0), DropZone::kInto);
    CheckZone("56px before bottom band is into", ContainerZone(56.0, 44.0), DropZone::kInto);
    CheckZone("56px bottom is after", ContainerZone(56.0, 52.0), DropZone::kAfter);
}

void RunSmallContainerCases() {
    CheckZone("20px row top is before", ContainerZone(20.0, 5.0), DropZone::kBefore);
    CheckZone("20px row middle is into", ContainerZone(20.0, 10.0), DropZone::kInto);
    CheckZone("20px row bottom is after", ContainerZone(20.0, 15.0), DropZone::kAfter);
    CheckZone("10px keeps a center zone", ContainerZone(10.0, 5.0), DropZone::kInto);
    CheckZone("10px top is before", ContainerZone(10.0, 2.0), DropZone::kBefore);
    CheckZone("10px bottom is after", ContainerZone(10.0, 8.0), DropZone::kAfter);
}

void RunOutOfRangeCases() {
    CheckZone("point above rect is before", ContainerZone(100.0, -20.0), DropZone::kBefore);
    CheckZone("point below rect is after", ContainerZone(100.0, 140.0), DropZone::kAfter);
    CheckZone("zero height rect does not fail", ContainerZone(0.0, 0.0), DropZone::kBefore);
}

void RunRootCases() {
    CheckZone("root top is into", ContainerZone(300.0, 0.0, false), DropZone::kInto);
    CheckZone("root center is into", ContainerZone(300.0, 150.0, false), DropZone::kInto);
    CheckZone("root bottom is into", ContainerZone(300.0, 300.0, false), DropZone::kInto);
}

void RunLeafCases() {
    CheckZone("leaf top is before", LeafZone(40.0, 0.0), DropZone::kBefore);
    CheckZone("leaf just above half is before", LeafZone(40.0, 19.0), DropZone::kBefore);
    CheckZone("leaf exact half is before", LeafZone(40.0, 20.0), DropZone::kBefore);
    CheckZone("leaf just below half is after", LeafZone(40.0, 21.0), DropZone::kAfter);
    CheckZone("leaf bottom is after", LeafZone(40.0, 40.0), DropZone::kAfter);
    CheckZone("leaf ignores allowSibling",
              ComputeDropZone(LayoutRect{0.0, 0.0, 100.0, 40.0}, LayoutPoint{0.0, 30.0}, false, false), DropZone::kAfter);
}

void RunOffsetCases() {
    const LayoutRect rect{50.0, 400.0, 200.0, 300.0};
    CheckZone("offset rect top band", ComputeDropZone(rect, LayoutPoint{60.0, 405.0}, true, true), DropZone::kBefore);
    CheckZone("offset rect center", ComputeDropZone(rect, LayoutPoint{60.0, 550.0}, true, true), DropZone::kInto);
    CheckZone("offset rect bottom band", ComputeDropZone(rect, LayoutPoint{60.0, 695.0}, true, true), DropZone::kAfter);
}

void RunIndicatorCases() {
    const LayoutRect rect{10.0, 20.0, 200.0, 100.0};
    const auto into = ComputeDropIndicator(rect, DropZone::kInto, true);
    CheckBool("container into draws full rect", !into.isLine && into.rect.height == 100.0, true);

    const auto before = ComputeDropIndicator(rect, DropZone::kBefore, true);
    CheckBool("container before draws top line", before.isLine && before.rect.y < rect.y + 1.0 && before.rect.y > rect.y - 3.0,
              true);
    CheckBool("container before spans container width", before.rect.width == rect.width, true);

    const auto after = ComputeDropIndicator(rect, DropZone::kAfter, true);
    CheckBool("container after draws bottom line", after.isLine && after.rect.y > rect.y + rect.height - 3.0 &&
                                                     after.rect.y < rect.y + rect.height + 1.0,
              true);

    const auto leaf = ComputeDropIndicator(rect, DropZone::kBefore, false);
    CheckBool("leaf before draws line", leaf.isLine, true);
}

std::vector<LayoutRect> ColumnRects() {
    return {LayoutRect{0.0, 0.0, 200.0, 40.0}, LayoutRect{0.0, 50.0, 200.0, 40.0}, LayoutRect{0.0, 100.0, 200.0, 40.0}};
}

std::vector<LayoutRect> RowRects() {
    return {LayoutRect{0.0, 0.0, 80.0, 30.0}, LayoutRect{100.0, 0.0, 80.0, 30.0}, LayoutRect{200.0, 0.0, 80.0, 30.0}};
}

std::vector<LayoutRect> GridRects() {
    return {LayoutRect{0.0, 0.0, 90.0, 40.0},   LayoutRect{100.0, 0.0, 90.0, 40.0},
            LayoutRect{0.0, 50.0, 90.0, 40.0},  LayoutRect{100.0, 50.0, 90.0, 40.0},
            LayoutRect{0.0, 100.0, 90.0, 40.0}};
}

void RunFlowLayoutCases() {
    const auto axisOf = [](const std::string& type, bool horizontal = false, int columns = 1) {
        return FlowLayoutOf(type, horizontal, columns).axis;
    };
    CheckBool("flow column is vertical", axisOf("Column") == FlowAxis::kVertical, true);
    CheckBool("flow row is horizontal", axisOf("Row") == FlowAxis::kHorizontal, true);
    CheckBool("flow is case insensitive", axisOf("column") == FlowAxis::kVertical, true);
    CheckBool("flow for is vertical", axisOf("For") == FlowAxis::kVertical, true);
    CheckBool("flow listview follows direction vertical", axisOf("ListView", false) == FlowAxis::kVertical, true);
    CheckBool("flow listview follows direction horizontal", axisOf("ListView", true) == FlowAxis::kHorizontal, true);
    CheckBool("flow flex follows direction horizontal", axisOf("Flex", true) == FlowAxis::kHorizontal, true);
    CheckBool("flow scrollview defaults vertical", axisOf("ScrollView") == FlowAxis::kVertical, true);
    CheckBool("flow grid is grid", axisOf("Grid", false, 3) == FlowAxis::kGrid, true);
    CheckBool("flow grid keeps columns", FlowLayoutOf("Grid", false, 3).columns == 3, true);
    CheckBool("flow grid with zero columns is vertical", axisOf("Grid", false, 0) == FlowAxis::kVertical, true);
    CheckBool("flow grid with one column is vertical", axisOf("Grid", false, 1) == FlowAxis::kVertical, true);
    CheckBool("flow page has no order", axisOf("Page") == FlowAxis::kNone, true);
    CheckBool("flow container has no order", axisOf("Container") == FlowAxis::kNone, true);
    CheckBool("flow stack has no order", axisOf("Stack") == FlowAxis::kNone, true);
    CheckBool("flow leaf has no order", axisOf("Button") == FlowAxis::kNone, true);
}

void RunVerticalIndexCases() {
    const FlowLayout flow{FlowAxis::kVertical, 1};
    const auto rects = ColumnRects();
    const auto at = [&](double y) { return ComputeInsertIndex(flow, rects, LayoutPoint{10.0, y}); };
    CheckIndex("vertical above first", at(-10.0), 0);
    CheckIndex("vertical upper half of first", at(10.0), 0);
    CheckIndex("vertical exact center of first", at(20.0), 1);
    CheckIndex("vertical lower half of first", at(30.0), 1);
    CheckIndex("vertical gap between first and second", at(45.0), 1);
    CheckIndex("vertical upper half of second", at(60.0), 1);
    CheckIndex("vertical lower half of second", at(75.0), 2);
    CheckIndex("vertical lower half of last", at(125.0), 3);
    CheckIndex("vertical below all", at(500.0), 3);
}

void RunHorizontalIndexCases() {
    const FlowLayout flow{FlowAxis::kHorizontal, 1};
    const auto rects = RowRects();
    const auto at = [&](double x) { return ComputeInsertIndex(flow, rects, LayoutPoint{x, 10.0}); };
    CheckIndex("horizontal left of first", at(-5.0), 0);
    CheckIndex("horizontal left half of first", at(30.0), 0);
    CheckIndex("horizontal right half of first", at(50.0), 1);
    CheckIndex("horizontal right half of second", at(150.0), 2);
    CheckIndex("horizontal right half of last", at(260.0), 3);
    CheckIndex("horizontal right of all", at(900.0), 3);
}

void RunGridIndexCases() {
    const FlowLayout flow{FlowAxis::kGrid, 2};
    const auto rects = GridRects();
    const auto at = [&](double x, double y) { return ComputeInsertIndex(flow, rects, LayoutPoint{x, y}); };
    CheckIndex("grid row 0 left half of first", at(10.0, 10.0), 0);
    CheckIndex("grid row 0 right half of first", at(60.0, 10.0), 1);
    CheckIndex("grid row 0 end of row", at(180.0, 10.0), 2);
    CheckIndex("grid gap belongs to next row", at(10.0, 45.0), 2);
    CheckIndex("grid row 1 second cell left half", at(110.0, 70.0), 3);
    CheckIndex("grid row 1 end of row", at(180.0, 70.0), 4);
    CheckIndex("grid last row left half", at(10.0, 110.0), 4);
    CheckIndex("grid last row end", at(150.0, 110.0), 5);
    CheckIndex("grid below all", at(10.0, 500.0), 5);
    CheckIndex("grid above all", at(10.0, -5.0), 0);
}

void RunIndexEdgeCases() {
    const std::vector<LayoutRect> none;
    CheckIndex("empty vertical", ComputeInsertIndex(FlowLayout{FlowAxis::kVertical, 1}, none, LayoutPoint{0.0, 0.0}), 0);
    CheckIndex("empty horizontal", ComputeInsertIndex(FlowLayout{FlowAxis::kHorizontal, 1}, none, LayoutPoint{0.0, 0.0}),
               0);
    CheckIndex("empty grid", ComputeInsertIndex(FlowLayout{FlowAxis::kGrid, 2}, none, LayoutPoint{0.0, 0.0}), 0);
    CheckIndex("unordered container appends", ComputeInsertIndex(FlowLayout{FlowAxis::kNone, 1}, ColumnRects(),
                                                                   LayoutPoint{10.0, 5.0}),
               3);
    CheckIndex("single column grid resolves like a column",
               ComputeInsertIndex(FlowLayoutOf("Grid", false, 1), ColumnRects(), LayoutPoint{10.0, 75.0}), 2);
}

void RunInsertPositionCases() {
    const std::vector<std::string> ids = {"A", "B", "C"};
    CheckIndex("position without moved node keeps index", ResolveInsertPosition(ids, 1, ""), 1);
    CheckIndex("position at end stays at end", ResolveInsertPosition(ids, 3, ""), 3);
    CheckIndex("position skips moved node", ResolveInsertPosition(ids, 1, "B"), 2);
    CheckIndex("position skips moved first node", ResolveInsertPosition(ids, 0, "A"), 1);
    CheckIndex("position skips moved last node to end", ResolveInsertPosition(ids, 2, "C"), 3);
    CheckIndex("position ignores moved node before index", ResolveInsertPosition(ids, 2, "A"), 2);
    CheckIndex("position ignores unknown moved node", ResolveInsertPosition(ids, 1, "Z"), 1);
}

void RunMarkerCases() {
    const FlowLayout column{FlowAxis::kVertical, 1};
    CheckBool("vertical marker before second", MarkerIs(column, ColumnRects(), 1, 0.0, 48.5, 200.0, 3.0), true);
    CheckBool("vertical marker at start", MarkerIs(column, ColumnRects(), 0, 0.0, -1.5, 200.0, 3.0), true);
    CheckBool("vertical marker at end", MarkerIs(column, ColumnRects(), 3, 0.0, 138.5, 200.0, 3.0), true);

    const FlowLayout row{FlowAxis::kHorizontal, 1};
    CheckBool("horizontal marker before third", MarkerIs(row, RowRects(), 2, 198.5, 0.0, 3.0, 30.0), true);
    CheckBool("horizontal marker at end", MarkerIs(row, RowRects(), 3, 278.5, 0.0, 3.0, 30.0), true);

    const FlowLayout grid{FlowAxis::kGrid, 2};
    CheckBool("grid marker before second cell", MarkerIs(grid, GridRects(), 1, 98.5, 0.0, 3.0, 40.0), true);
    CheckBool("grid marker at row start sits after previous row end", MarkerIs(grid, GridRects(), 2, 188.5, 0.0, 3.0, 40.0),
              true);
    CheckBool("grid marker at start", MarkerIs(grid, GridRects(), 0, -1.5, 0.0, 3.0, 40.0), true);
    CheckBool("grid marker at end", MarkerIs(grid, GridRects(), 5, 88.5, 100.0, 3.0, 40.0), true);

    LayoutRect ignored;
    CheckBool("marker absent for empty container", ComputeInsertMarker(column, {}, 0, ignored), false);
    CheckBool("marker absent for unordered container",
              ComputeInsertMarker(FlowLayout{FlowAxis::kNone, 1}, ColumnRects(), 1, ignored), false);
}

}

int main() {
    std::cout << "ComputeDropZone tall container\n";
    RunTallContainerCases();
    std::cout << "ComputeDropZone minimum container\n";
    RunMinimumContainerCases();
    std::cout << "ComputeDropZone small container\n";
    RunSmallContainerCases();
    std::cout << "ComputeDropZone out of range\n";
    RunOutOfRangeCases();
    std::cout << "ComputeDropZone root\n";
    RunRootCases();
    std::cout << "ComputeDropZone leaf\n";
    RunLeafCases();
    std::cout << "ComputeDropZone offset rect\n";
    RunOffsetCases();
    std::cout << "ComputeDropIndicator\n";
    RunIndicatorCases();
    std::cout << "FlowLayoutOf\n";
    RunFlowLayoutCases();
    std::cout << "ComputeInsertIndex vertical\n";
    RunVerticalIndexCases();
    std::cout << "ComputeInsertIndex horizontal\n";
    RunHorizontalIndexCases();
    std::cout << "ComputeInsertIndex grid\n";
    RunGridIndexCases();
    std::cout << "ComputeInsertIndex edges\n";
    RunIndexEdgeCases();
    std::cout << "ResolveInsertPosition\n";
    RunInsertPositionCases();
    std::cout << "ComputeInsertMarker\n";
    RunMarkerCases();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
