
#include <jni.h>
#include <string>
#include <android/log.h>
#include <fstream>
#include <memory>

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_BiosActivity_loadBios(JNIEnv *env, jobject thiz, jstring biosFilePath) {
    const char *biosPath = env->GetStringUTFChars(biosFilePath, nullptr);
    if (biosPath == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "BIOS", "Failed to get the BIOS file path.");
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "BIOS", "Loading BIOS from: %s", biosPath);

    
    if (strstr(biosPath, "Complex_4627v1.03.bin") == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "BIOS", "Invalid BIOS file. Expected 'Complex_4627v1.03.bin'. Found: %s", biosPath);
        env->ReleaseStringUTFChars(biosFilePath, biosPath);
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "BIOS", "BIOS loaded successfully!");

    env->ReleaseStringUTFChars(biosFilePath, biosPath);
    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_BiosActivity_loadBootRom(JNIEnv *env, jobject thiz, jstring bootRomFilePath) {
    const char *bootRomPath = env->GetStringUTFChars(bootRomFilePath, nullptr);
    if (bootRomPath == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "Boot ROM", "Failed to get the Boot ROM file path.");
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "Boot ROM", "Loading Boot ROM from: %s", bootRomPath);

    if (strstr(bootRomPath, "mcpx_1.0.bin") == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "Boot ROM", "Invalid Boot ROM file. Expected 'mcpx_1.0.bin'. Found: %s", bootRomPath);
        env->ReleaseStringUTFChars(bootRomFilePath, bootRomPath);
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "Boot ROM", "Boot ROM loaded successfully!");

    env->ReleaseStringUTFChars(bootRomFilePath, bootRomPath);
    return JNI_TRUE;  
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_BiosActivity_loadHddImage(JNIEnv *env, jobject thiz, jstring hddImageFilePath) {
    const char *hddImagePath = env->GetStringUTFChars(hddImageFilePath, nullptr);
    if (hddImagePath == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "HDD Image", "Failed to get the HDD image file path.");
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "HDD Image", "Loading Xbox HDD Image from: %s", hddImagePath);

    if (strstr(hddImagePath, "xbox_hdd.qcow2") == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "HDD Image", "Invalid Xbox HDD Image file. Expected 'xbox_hdd.qcow2'. Found: %s", hddImagePath);
        env->ReleaseStringUTFChars(hddImageFilePath, hddImagePath);
        return JNI_FALSE; 
    }

    __android_log_print(ANDROID_LOG_INFO, "HDD Image", "Xbox HDD Image loaded successfully!");

    env->ReleaseStringUTFChars(hddImageFilePath, hddImagePath);
    return JNI_TRUE;  
}