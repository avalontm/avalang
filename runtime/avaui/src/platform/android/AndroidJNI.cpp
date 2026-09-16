#include "AndroidJNI.h"
#include "AndroidJNIInternal.h"

#include <mutex>
#include <unordered_map>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

JavaVM* g_vm = nullptr;
jobject g_bridge = nullptr;
std::mutex g_bridgeMutex;

JNIEnv* AttachEnv(bool& outShouldDetach) {
    outShouldDetach = false;
    if (!g_vm) return nullptr;

    JNIEnv* env = nullptr;
    jint status = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != 0) return nullptr;
        outShouldDetach = true;
    } else if (status != JNI_OK) {
        return nullptr;
    }
    return env;
}

void DetachIfNeeded(bool shouldDetach) {
    if (shouldDetach && g_vm) g_vm->DetachCurrentThread();
}

jstring ToJString(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

std::string ToStdString(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string result(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(s, chars);
    return result;
}

jmethodID FindMethod(JNIEnv* env, jobject obj, const char* name, const char* signature) {
    if (!obj) return nullptr;
    jclass cls = env->GetObjectClass(obj);
    if (!cls) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, signature);
    env->DeleteLocalRef(cls);
    return id;
}

}

void AndroidJNI_SetVM(JavaVM* vm) {
    g_vm = vm;
}

void AndroidJNI_SetBridge(JNIEnv* env, jobject bridge) {
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (g_bridge) env->DeleteGlobalRef(g_bridge);
    g_bridge = bridge ? env->NewGlobalRef(bridge) : nullptr;
}

void Bridge_ShowKeyboard() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "showKeyboard", "()V");
    if (m) env->CallVoidMethod(g_bridge, m);
    DetachIfNeeded(detach);
}

void Bridge_HideKeyboard() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "hideKeyboard", "()V");
    if (m) env->CallVoidMethod(g_bridge, m);
    DetachIfNeeded(detach);
}

void Bridge_QueryDisplayMetrics(int& width, int& height, float& density, int& densityDpi) {
    width = 0; height = 0; density = 1.0f; densityDpi = 160;
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "queryDisplayMetrics", "()[F");
    if (m) {
        auto arr = static_cast<jfloatArray>(env->CallObjectMethod(g_bridge, m));
        if (arr && env->GetArrayLength(arr) >= 4) {
            jfloat buf[4];
            env->GetFloatArrayRegion(arr, 0, 4, buf);
            width = static_cast<int>(buf[0]);
            height = static_cast<int>(buf[1]);
            density = buf[2];
            densityDpi = static_cast<int>(buf[3]);
        }
        if (arr) env->DeleteLocalRef(arr);
    }
    DetachIfNeeded(detach);
}

void Bridge_ClipboardSetText(const std::string& text) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "clipboardSetText", "(Ljava/lang/String;)V");
    if (m) {
        jstring jtext = ToJString(env, text);
        env->CallVoidMethod(g_bridge, m, jtext);
        env->DeleteLocalRef(jtext);
    }
    DetachIfNeeded(detach);
}

std::string Bridge_ClipboardGetText() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return {};
    std::string result;
    jmethodID m = FindMethod(env, g_bridge, "clipboardGetText", "()Ljava/lang/String;");
    if (m) {
        auto jtext = static_cast<jstring>(env->CallObjectMethod(g_bridge, m));
        result = ToStdString(env, jtext);
        if (jtext) env->DeleteLocalRef(jtext);
    }
    DetachIfNeeded(detach);
    return result;
}

bool Bridge_ClipboardHasText() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return false;
    bool result = false;
    jmethodID m = FindMethod(env, g_bridge, "clipboardHasText", "()Z");
    if (m) result = env->CallBooleanMethod(g_bridge, m) != JNI_FALSE;
    DetachIfNeeded(detach);
    return result;
}

std::string Bridge_FilesDir() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return {};
    std::string result;
    jmethodID m = FindMethod(env, g_bridge, "filesDir", "()Ljava/lang/String;");
    if (m) {
        auto jtext = static_cast<jstring>(env->CallObjectMethod(g_bridge, m));
        result = ToStdString(env, jtext);
        if (jtext) env->DeleteLocalRef(jtext);
    }
    DetachIfNeeded(detach);
    return result;
}

std::string Bridge_CacheDir() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return {};
    std::string result;
    jmethodID m = FindMethod(env, g_bridge, "cacheDir", "()Ljava/lang/String;");
    if (m) {
        auto jtext = static_cast<jstring>(env->CallObjectMethod(g_bridge, m));
        result = ToStdString(env, jtext);
        if (jtext) env->DeleteLocalRef(jtext);
    }
    DetachIfNeeded(detach);
    return result;
}

void Bridge_ShowNotification(const std::string& title, const std::string& body) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "showNotification", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (m) {
        jstring jtitle = ToJString(env, title);
        jstring jbody = ToJString(env, body);
        env->CallVoidMethod(g_bridge, m, jtitle, jbody);
        env->DeleteLocalRef(jtitle);
        env->DeleteLocalRef(jbody);
    }
    DetachIfNeeded(detach);
}

void Bridge_OpenUri(const std::string& uri) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "openUri", "(Ljava/lang/String;)V");
    if (m) {
        jstring juri = ToJString(env, uri);
        env->CallVoidMethod(g_bridge, m, juri);
        env->DeleteLocalRef(juri);
    }
    DetachIfNeeded(detach);
}

void Bridge_RequestPermission(const std::string& permission, int requestCode) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "requestPermission", "(Ljava/lang/String;I)V");
    if (m) {
        jstring jperm = ToJString(env, permission);
        env->CallVoidMethod(g_bridge, m, jperm, static_cast<jint>(requestCode));
        env->DeleteLocalRef(jperm);
    }
    DetachIfNeeded(detach);
}

bool Bridge_CheckPermission(const std::string& permission) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return false;
    bool result = false;
    jmethodID m = FindMethod(env, g_bridge, "checkPermission", "(Ljava/lang/String;)Z");
    if (m) {
        jstring jperm = ToJString(env, permission);
        result = env->CallBooleanMethod(g_bridge, m, jperm) != JNI_FALSE;
        env->DeleteLocalRef(jperm);
    }
    DetachIfNeeded(detach);
    return result;
}

void Bridge_CanvasBeginFrame(int width, int height) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasBeginFrame", "(II)V");
    if (m) env->CallVoidMethod(g_bridge, m, static_cast<jint>(width), static_cast<jint>(height));
    DetachIfNeeded(detach);
}

void Bridge_CanvasEndFrame() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasEndFrame", "()V");
    if (m) env->CallVoidMethod(g_bridge, m);
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawRect(float x, float y, float width, float height,
                            uint32_t fillColor, uint32_t borderColor, float borderWidth, float borderRadius) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawRect", "(FFFFIIFF)V");
    if (m) {
        env->CallVoidMethod(g_bridge, m, x, y, width, height,
                             static_cast<jint>(fillColor), static_cast<jint>(borderColor),
                             borderWidth, borderRadius);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawEllipse(float cx, float cy, float rx, float ry,
                               uint32_t fillColor, uint32_t borderColor, float borderWidth) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawEllipse", "(FFFFIIF)V");
    if (m) {
        env->CallVoidMethod(g_bridge, m, cx, cy, rx, ry,
                             static_cast<jint>(fillColor), static_cast<jint>(borderColor), borderWidth);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawText(float x, float y, const std::string& text,
                            float fontSize, const std::string& fontName, uint32_t color,
                            float maxWidth, bool wrap) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawText", "(FFLjava/lang/String;FLjava/lang/String;IFZ)V");
    if (m) {
        jstring jtext = ToJString(env, text);
        jstring jfont = ToJString(env, fontName);
        env->CallVoidMethod(g_bridge, m, x, y, jtext, fontSize, jfont,
                             static_cast<jint>(color), maxWidth, static_cast<jboolean>(wrap));
        env->DeleteLocalRef(jtext);
        env->DeleteLocalRef(jfont);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawImage(float x, float y, float width, float height, const std::string& imagePath) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawImage", "(FFFFLjava/lang/String;)V");
    if (m) {
        jstring jpath = ToJString(env, imagePath);
        env->CallVoidMethod(g_bridge, m, x, y, width, height, jpath);
        env->DeleteLocalRef(jpath);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawButton(float x, float y, float width, float height, const std::string& text,
                              float fontSize, const std::string& fontName,
                              uint32_t textColor, uint32_t fillColor, uint32_t borderColor,
                              float borderWidth, float borderRadius, bool disabled) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawButton",
                              "(FFFFLjava/lang/String;FLjava/lang/String;IIIFFZ)V");
    if (m) {
        jstring jtext = ToJString(env, text);
        jstring jfont = ToJString(env, fontName);
        env->CallVoidMethod(g_bridge, m, x, y, width, height, jtext, fontSize, jfont,
                             static_cast<jint>(textColor), static_cast<jint>(fillColor),
                             static_cast<jint>(borderColor), borderWidth, borderRadius,
                             static_cast<jboolean>(disabled));
        env->DeleteLocalRef(jtext);
        env->DeleteLocalRef(jfont);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasDrawLink(float x, float y, const std::string& text,
                            float fontSize, const std::string& fontName, uint32_t color) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasDrawLink", "(FFLjava/lang/String;FLjava/lang/String;I)V");
    if (m) {
        jstring jtext = ToJString(env, text);
        jstring jfont = ToJString(env, fontName);
        env->CallVoidMethod(g_bridge, m, x, y, jtext, fontSize, jfont, static_cast<jint>(color));
        env->DeleteLocalRef(jtext);
        env->DeleteLocalRef(jfont);
    }
    DetachIfNeeded(detach);
}

void Bridge_CanvasPushClip(float x, float y, float width, float height) {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasPushClip", "(FFFF)V");
    if (m) env->CallVoidMethod(g_bridge, m, x, y, width, height);
    DetachIfNeeded(detach);
}

void Bridge_CanvasPopClip() {
    bool detach;
    JNIEnv* env = AttachEnv(detach);
    if (!env) return;
    jmethodID m = FindMethod(env, g_bridge, "canvasPopClip", "()V");
    if (m) env->CallVoidMethod(g_bridge, m);
    DetachIfNeeded(detach);
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
