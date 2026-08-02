#ifndef AVA_UI_COMMON_NONCOPYABLE_H
#define AVA_UI_COMMON_NONCOPYABLE_H

namespace avalang {
namespace ui {
namespace common {

// Shared by ComponentTree, LayoutEngine, RenderTree, SceneGraph, etc.
// (Phases 2-7) -- those types own graph state and must not be copied.
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

} // namespace common
} // namespace ui
} // namespace avalang

#endif // AVA_UI_COMMON_NONCOPYABLE_H
