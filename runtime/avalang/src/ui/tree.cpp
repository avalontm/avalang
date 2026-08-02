#include "ui/tree.h"

// ComponentTree is currently a thin owner of the root Component (see
// tree.h). Kept as its own translation unit, separate from component.cpp,
// so tree-level operations (traversal, diffing, serialization) that the
// designer/renderer will need can grow here without touching Component
// itself.

namespace ava {
namespace ui {

} // namespace ui
} // namespace ava
