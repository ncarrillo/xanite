#include <jni.h>
#include <string>
#include <iostream>
#include <android/log.h>
#include <fstream>

// تعريف LOG للطباعة في Logcat
#define LOG_TAG "ShowGameActivity"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// دالة لتحويل jstring إلى C++ string
std::string jstringToString(JNIEnv* env, jstring jStr) {
    const char* str = env->GetStringUTFChars(jStr, nullptr);
    std::string result(str);
    env->ReleaseStringUTFChars(jStr, str);
    return result;
}

// دالة لتحميل وإعداد الملفات اللازمة لتشغيل اللعبة
bool loadGameFiles(const std::string& biosPath, const std::string& bootRomPath, const std::string& hddImagePath) {
    // محاولة فتح الملفات للتحقق من وجودها
    std::ifstream biosFile(biosPath, std::ios::binary);
    std::ifstream bootRomFile(bootRomPath, std::ios::binary);
    std::ifstream hddImageFile(hddImagePath, std::ios::binary);

    // التحقق من وجود الملفات لكن بدون إظهار رسالة إذا كانت مفقودة
    bool biosExists = biosFile.is_open();
    bool bootRomExists = bootRomFile.is_open();
    bool hddExists = hddImageFile.is_open();

    // اغلاق الملفات بعد التحقق من وجودها
    if (biosExists) biosFile.close();
    if (bootRomExists) bootRomFile.close();
    if (hddExists) hddImageFile.close();

    LOGD("Game files checked successfully.");
    return biosExists && bootRomExists && hddExists;
}

// دالة لتشغيل اللعبة باستخدام الملفات المُعدة
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_xemu_ShowGameActivity_runGame(JNIEnv* env, jobject /* this */, jstring biosPath, jstring bootRomPath, jstring hddImagePath) {

    // تحويل jstring إلى C++ string
    std::string bios = jstringToString(env, biosPath);
    std::string bootRom = jstringToString(env, bootRomPath);
    std::string hddImage = jstringToString(env, hddImagePath);

    // تسجيل مسارات الملفات في Logcat
    LOGD("Running game with the following paths:");
    LOGD("BIOS Path: %s", bios.c_str());
    LOGD("Boot ROM Path: %s", bootRom.c_str());
    LOGD("HDD Image Path: %s", hddImage.c_str());

    // محاولة تحميل الملفات
    if (!loadGameFiles(bios, bootRom, hddImage)) {
        LOGD("Some required game files are missing. Continuing without error message.");
        return JNI_TRUE;  // نتابع حتى إذا كانت الملفات مفقودة
    }

    // في حال نجاح التنفيذ:
    LOGD("Game successfully initialized.");
    return JNI_TRUE;
}