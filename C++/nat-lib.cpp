#include <jni.h>
#include <string>
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

// استخدام تسجيل رسائل Log للطباعة في Logcat
#define LOG_TAG "native-lib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// دالة لتحويل النص إلى أحرف صغيرة
std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

// دالة للتحقق مما إذا كان الملف هو ملف لعبة مدعوم (.iso أو .xiso)
bool isSupportedGameFile(const std::string& filePath) {
    std::string lowerPath = toLower(filePath);
    return (lowerPath.find(".iso") != std::string::npos || lowerPath.find(".xiso") != std::string::npos);
}

// دالة لتحميل محتوى ملف اللعبة بشكل آمن (مع معالجة الأخطاء)
bool loadGameFile(const std::string& filePath, std::string& outContents) {
    std::ifstream gameFile(filePath, std::ios::in | std::ios::binary);
    if (!gameFile.is_open()) {
        LOGE("Failed to open the game file: %s", filePath.c_str());
        return false;
    }

    // قراءة محتويات الملف إلى سلسلة نصية
    std::stringstream buffer;
    buffer << gameFile.rdbuf();
    outContents = buffer.str();
    gameFile.close();

    LOGD("Successfully loaded game file: %s", filePath.c_str());
    return true;
}

// دالة لقراءة الملفات بشكل آمن باستخدام أكثر من معاملة
bool readGameFileSafe(const std::string& path, std::string& contents) {
    try {
        if (!isSupportedGameFile(path)) {
            LOGE("Unsupported file format: %s", path.c_str());
            return false;
        }

        // محاولة تحميل ملف اللعبة
        if (!loadGameFile(path, contents)) {
            return false;
        }

        LOGD("Game file processed successfully: %s", path.c_str());
        return true;
    } catch (const std::exception& e) {
        LOGE("Exception occurred: %s", e.what());
        return false;
    }
}

// دالة لعملية معالجة اللعبة
extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameSelectionActivity_nativeProcessGameFile(JNIEnv* env, jobject /* this */, jstring filePath) {
    const char* path = env->GetStringUTFChars(filePath, nullptr);  // تحويل مسار الملف إلى C-style string

    if (path == nullptr) {
        LOGE("Null file path provided.");
        return;
    }

    // تحويل الملف إلى سلسلة C++
    std::string gamePath(path);
    
    // استخدام الدالة الخاصة بالتحقق من الملف
    std::string fileContents;
    if (!readGameFileSafe(gamePath, fileContents)) {
        LOGE("Failed to process the game file: %s", gamePath.c_str());
    } else {
        LOGD("Successfully processed the game file: %s", gamePath.c_str());
        // يمكن استخدام محتويات الملف هنا لتنفيذ العمليات المطلوبة
    }

    // تحرير الذاكرة
    env->ReleaseStringUTFChars(filePath, path);
}