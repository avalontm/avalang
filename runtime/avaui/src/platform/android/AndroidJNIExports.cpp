#include "AndroidJNIInternal.h"
#include "AndroidAppLifecycle.h"
#include "AndroidMobileSurface.h"
#include "AndroidSafeArea.h"
#include "AndroidVirtualKeyboard.h"
#include "AndroidInputState.h"
#include "AndroidPermissions.h"
#include "AndroidBackNavigation.h"

#include <jni.h>
#include <cstdint>
#include <vector>

using avalang::ui::platform::android::AndroidJNI_SetBridge;
using avalang::ui::platform::android::AndroidJNI_SetVM;
using avalang::ui::platform::android::GetAppLifecycle;
using avalang::ui::platform::android::GetMobileSurface;
using avalang::ui::platform::android::GetSafeArea;
using avalang::ui::platform::android::GetVirtualKeyboard;
using avalang::ui::platform::android::GetPermissions;
using avalang::ui::platform::android::AndroidInput_SetTouchPoints;
using avalang::ui::platform::android::AndroidInput_PushCommittedText;
using avalang::ui::platform::android::AndroidInput_SetImeComposition;
using avalang::ui::platform::android::AndroidNav_OnBackPressed;

namespace {

std::string JStringToStd(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string result(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(s, chars);
    return result;
}

}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    AndroidJNI_SetVM(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeSetBridge(
    JNIEnv* env, jobject, jobject bridge) {
    AndroidJNI_SetBridge(env, bridge);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnCreate(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Created);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnStart(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Started);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnResume(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Resumed);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnPause(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Paused);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnStop(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Stopped);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnDestroy(JNIEnv*, jobject) {
    GetAppLifecycle().SetState(ava::platform::mobile::AppLifecycleState::Destroyed);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnLowMemory(JNIEnv*, jobject) {
    GetAppLifecycle().NotifyLowMemory();
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnSurfaceCreated(
    JNIEnv*, jobject, jlong nativeHandle, jint width, jint height) {
    GetMobileSurface().NotifyCreated(reinterpret_cast<void*>(nativeHandle), width, height);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnSurfaceResized(
    JNIEnv*, jobject, jint width, jint height) {
    GetMobileSurface().NotifyResized(width, height);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnSurfaceDestroyed(JNIEnv*, jobject) {
    GetMobileSurface().NotifyDestroyed();
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnSafeAreaChanged(
    JNIEnv*, jobject, jfloat top, jfloat right, jfloat bottom, jfloat left) {
    GetSafeArea().NotifyChanged(top, right, bottom, left);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnKeyboardVisibilityChanged(
    JNIEnv*, jobject, jboolean visible, jint occupiedHeight) {
    GetVirtualKeyboard().NotifyVisibility(visible != JNI_FALSE, occupiedHeight);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnTouchEvent(
    JNIEnv* env, jobject, jlongArray ids, jintArray xs, jintArray ys) {
    jsize count = env->GetArrayLength(ids);
    std::vector<jlong> idBuf(static_cast<size_t>(count));
    std::vector<jint> xBuf(static_cast<size_t>(count));
    std::vector<jint> yBuf(static_cast<size_t>(count));
    env->GetLongArrayRegion(ids, 0, count, idBuf.data());
    env->GetIntArrayRegion(xs, 0, count, xBuf.data());
    env->GetIntArrayRegion(ys, 0, count, yBuf.data());

    std::vector<ava::platform::ui::NativeTouchPoint> points;
    points.reserve(static_cast<size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        ava::platform::ui::NativeTouchPoint point;
        point.id = static_cast<uint64_t>(idBuf[static_cast<size_t>(i)]);
        point.x = xBuf[static_cast<size_t>(i)];
        point.y = yBuf[static_cast<size_t>(i)];
        points.push_back(point);
    }
    AndroidInput_SetTouchPoints(points);
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnTextCommitted(
    JNIEnv* env, jobject, jstring text) {
    AndroidInput_PushCommittedText(JStringToStd(env, text));
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnImeComposition(
    JNIEnv* env, jobject, jboolean active, jstring text, jint cursor) {
    AndroidInput_SetImeComposition(active != JNI_FALSE, JStringToStd(env, text), cursor);
}

JNIEXPORT jboolean JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnBackPressed(JNIEnv*, jobject) {
    return AndroidNav_OnBackPressed() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_avalang_ui_AvaHostBridge_nativeOnPermissionResult(
    JNIEnv* env, jobject, jstring permission, jboolean granted) {
    GetPermissions().NotifyResult(JStringToStd(env, permission), granted != JNI_FALSE);
}

}
