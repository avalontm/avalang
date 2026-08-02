#ifndef AVA_PLATFORM_SERVICES_UI_UIPLATFORMINTERFACES_H
#define AVA_PLATFORM_SERVICES_UI_UIPLATFORMINTERFACES_H

// Phase 0 stub. Aggregates the PAL extension points reserved for
// avalang.ui.dll. These are OS-level UI *services* (window, input,
// clipboard, timers, etc.), not a platform of their own -- they live
// under interfaces/services/ui/, alongside Windows/Linux/macOS, which
// are the actual platforms. None of these are consumed by IPlatform
// yet -- wiring them in is a later phase (see docs/Platform_Foundation.md).

#include "IWindow.h"
#include "IMouse.h"
#include "IKeyboard.h"
#include "ICursor.h"
#include "IClipboard.h"
#include "IRenderSurface.h"
#include "ITimer.h"
#include "IDisplay.h"
#include "IPlatformServices.h"

#endif // AVA_PLATFORM_SERVICES_UI_UIPLATFORMINTERFACES_H
