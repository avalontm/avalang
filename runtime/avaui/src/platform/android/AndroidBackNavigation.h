#pragma once

#include "navigation/INavigator.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidNav_SetActiveNavigator(navigation::INavigator* navigator);
bool AndroidNav_OnBackPressed();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
