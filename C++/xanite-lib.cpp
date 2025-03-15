#include <jni.h>
#include <android/log.h>

#define LOG_TAG "xanite"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT void JNICALL
Java_com_example_xemu_MainActivity_initializeEmulator(JNIEnv *env, jobject thiz) {
    LOGI("Initializing xanite Emulator...");

}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_xemu_MainActivity_loadPackages(JNIEnv *env, jobject thiz) {
    LOGI("Loading required packages for xanite...");
 }   

extern "C"
JNIEXPORT void JNICALL
Java_com_example_xemu_MainActivity_createFolderInNativeCode(JNIEnv *env, jobject thiz) {
    jclass activityClass = env->GetObjectClass(thiz);
    jmethodID createFolderMethod = env->GetMethodID(activityClass, "createxaniteAndroidFolder", "()V");
    if (createFolderMethod == nullptr) {
        LOGI("Method not found");
        return;
    }
    env->CallVoidMethod(thiz, createFolderMethod);
}