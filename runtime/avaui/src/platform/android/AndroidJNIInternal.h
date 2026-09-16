#pragma once

#include <jni.h>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidJNI_SetVM(JavaVM* vm);
void AndroidJNI_SetBridge(JNIEnv* env, jobject bridge);

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
