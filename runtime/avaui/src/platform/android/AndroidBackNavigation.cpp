#include "AndroidBackNavigation.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

navigation::INavigator* g_activeNavigator = nullptr;

}

void AndroidNav_SetActiveNavigator(navigation::INavigator* navigator) {
    g_activeNavigator = navigator;
}

bool AndroidNav_OnBackPressed() {
    if (!g_activeNavigator) return false;
    if (!g_activeNavigator->CanBack()) return false;
    return g_activeNavigator->Back();
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
