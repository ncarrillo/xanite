#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <fstream>
#include <vector>
#include <string>

#define LOG_TAG "LOAD_LIB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

std::string biosPath = "Complex_4627v1.03.bin";
std::string mcpxPath = "mcpx_1.0.bin";
std::string hddPath = "xbox_hdd.qcow2";

bool loadFileFromAssets(JNIEnv *env, AAssetManager *assetManager, const std::string &fileName, const std::string &outputPath) {
    AAsset *asset = AAssetManager_open(assetManager, fileName.c_str(), AASSET_MODE_STREAMING);
    if (!asset) {
        LOGE("Failed to open file: %s from assets", fileName.c_str());
        return false;
    }

    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        LOGE("Failed to create output file: %s", outputPath.c_str());
        AAsset_close(asset);
        return false;
    }

    const size_t bufferSize = 1024;
    std::vector<char> buffer(bufferSize);
    int bytesRead;
    while ((bytesRead = AAsset_read(asset, buffer.data(), bufferSize)) > 0) {
        outputFile.write(buffer.data(), bytesRead);
    }

    AAsset_close(asset);
    outputFile.close();

    LOGI("Successfully loaded %s to %s", fileName.c_str(), outputPath.c_str());
    return true;
}

bool isValidIsoFile(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        LOGE("Failed to open game file: %s", filePath.c_str());
        return false;
    }

    LOGI("Game file is valid: %s", filePath.c_str());
    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GamePlayActivity_loadRequiredFiles(JNIEnv *env, jobject thiz, jobject assetManager, jstring gameFilePath) {
    const char *gameFilePathCStr = env->GetStringUTFChars(gameFilePath, nullptr);
    if (!gameFilePathCStr) {
        LOGE("Failed to get game file path.");
        return JNI_FALSE;
    }

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    if (mgr == nullptr) {
        LOGE("Failed to obtain AAssetManager.");
        env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
        return JNI_FALSE;
    }

    std::string appDir = "/data/data/com.example.xemu/files/";

    if (!loadFileFromAssets(env, mgr, biosPath, appDir + biosPath)) {
        LOGE("Failed to load BIOS.");
        env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
        return JNI_FALSE;
    }

    if (!loadFileFromAssets(env, mgr, mcpxPath, appDir + mcpxPath)) {
        LOGE("Failed to load MCPX.");
        env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
        return JNI_FALSE;
    }

    if (!loadFileFromAssets(env, mgr, hddPath, appDir + hddPath)) {
        LOGE("Failed to load HDD.");
        env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
        return JNI_FALSE;
    }

    if (!isValidIsoFile(gameFilePathCStr)) {
        LOGE("Invalid game file.");
        env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
        return JNI_FALSE;
    }

    LOGI("All required files loaded successfully!");
    env->ReleaseStringUTFChars(gameFilePath, gameFilePathCStr);
    return JNI_TRUE;
}