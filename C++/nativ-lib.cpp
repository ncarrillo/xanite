#include <jni.h>
#include <string>
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdint>

#ifndef LOG_TAG
#define LOG_TAG "GamePlayActivity"
#endif

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GamePlayActivity_nativeLoadGame(JNIEnv *env, jobject thiz, jstring iso_file_path, jobject progressBar) {
    const char *isoPath = env->GetStringUTFChars(iso_file_path, nullptr);
    if (isoPath == nullptr) {
        LOGE("Failed to get ISO file path string.");
        return JNI_FALSE;
    }
    LOGI("Loading game from: %s", isoPath);

    // فتح ملف ISO
    std::ifstream file(isoPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open ISO file: %s", isoPath);
        env->ReleaseStringUTFChars(iso_file_path, isoPath);
        return JNI_FALSE;
    }
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    env->ReleaseStringUTFChars(iso_file_path, isoPath);

    // تحديث شريط التقدم
    jclass progressBarClass = env->GetObjectClass(progressBar);
    jmethodID setProgressMethod = env->GetMethodID(progressBarClass, "setProgress", "(I)V");
    int progress = 0;

    // قراءة الملف
    const size_t bufferSize = 8192;
    char buffer[bufferSize];
    size_t readBytes = 0;

    // محاولة القراءة مع التعامل مع الأخطاء
    while (file) {
        try {
            file.read(buffer, bufferSize);
            size_t bytesRead = file.gcount();
            if (bytesRead == 0) {
                LOGE("Error reading ISO file or file is empty at position %zu", readBytes);
                break; // الخروج إذا كان هناك خطأ في القراءة
            }
            readBytes += bytesRead;
            progress = (int)((readBytes * 100) / fileSize);
            if (progress > 100) progress = 100;

            // تحديث شريط التقدم
            env->CallVoidMethod(progressBar, setProgressMethod, progress);
        } catch (const std::exception& e) {
            LOGE("Exception occurred while reading the ISO: %s", e.what());
            break; // التوقف إذا حدث استثناء
        }
    }

    // إغلاق الملف بعد الانتهاء
    file.close();

    // إكمال شريط التقدم إلى 100%
    env->CallVoidMethod(progressBar, setProgressMethod, 100);

    LOGI("File read complete: %zu bytes", readBytes);

    // إبلاغ GameDisplayActivity بتحميل البيانات
    jclass gameDisplayActivityClass = env->GetObjectClass(thiz);
    jmethodID startGameDisplayMethod = env->GetMethodID(gameDisplayActivityClass, "startGameDisplay", "()V");
    if (startGameDisplayMethod != nullptr) {
        env->CallVoidMethod(thiz, startGameDisplayMethod);
    } else {
        LOGE("startGameDisplay method not found in GameDisplayActivity.");
    }

    return JNI_TRUE;
}