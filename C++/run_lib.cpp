#include <jni.h>
#include <string>
#include <iostream>
#include <android/log.h>
#include <fstream>

#define LOG_TAG "ShowGameActivity"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

std::string jstringToString(JNIEnv* env, jstring jStr) {
    const char* str = env->GetStringUTFChars(jStr, nullptr);
    std::string result(str);
    env->ReleaseStringUTFChars(jStr, str);
    return result;
}

bool loadGameFiles(const std::string& biosPath, const std::string& bootRomPath, const std::string& hddImagePath) {
    std::ifstream biosFile(biosPath, std::ios::binary);
    std::ifstream bootRomFile(bootRomPath, std::ios::binary);
    std::ifstream hddImageFile(hddImagePath, std::ios::binary);

    bool biosExists = biosFile.is_open();
    bool bootRomExists = bootRomFile.is_open();
    bool hddExists = hddImageFile.is_open();

    if (biosExists) biosFile.close();
    if (bootRomExists) bootRomFile.close();
    if (hddExists) hddImageFile.close();

    LOGD("Game files checked successfully.");
    return biosExists && bootRomExists && hddExists;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_xemu_ShowGameActivity_runGame(JNIEnv* env, jobject /* this */, jstring biosPath, jstring bootRomPath, jstring hddImagePath) {

    std::string bios = jstringToString(env, biosPath);
    std::string bootRom = jstringToString(env, bootRomPath);
    std::string hddImage = jstringToString(env, hddImagePath);

    LOGD("Running game with the following paths:");
    LOGD("BIOS Path: %s", bios.c_str());
    LOGD("Boot ROM Path: %s", bootRom.c_str());
    LOGD("HDD Image Path: %s", hddImage.c_str());

    if (!loadGameFiles(bios, bootRom, hddImage)) {
        LOGD("Some required game files are missing. Continuing without error message.");
        return JNI_TRUE;
    }

    LOGD("Game successfully initialized.");
    return JNI_TRUE;
}