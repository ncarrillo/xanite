#include <jni.h>
#include <android/log.h>
#include <string>

#define LOG_TAG "nativ_lib"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Function to load the game
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GamePlayViewModel_nativLoadGame(JNIEnv *env, jobject thiz, jobject context, jobject uri) {
    LOGE("Game loading from nativ code");

    // Example: Print the URI for verification
    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID toStringMethod = env->GetMethodID(uriClass, "toString", "()Ljava/lang/String;");
    jstring uriString = (jstring) env->CallObjectMethod(uri, toStringMethod);
    const char *uriCStr = env->GetStringUTFChars(uriString, nullptr);
    LOGE("URI: %s", uriCStr);
    env->ReleaseStringUTFChars(uriString, uriCStr);

    // Add actual game loading logic here

    // Return success (JNI_TRUE) if loading was successful
    return JNI_TRUE;
}

// Example function to return a string
extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_xemu_GamePlayViewModel_getExampleString(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello";
    return env->NewStringUTF(hello.c_str());
}