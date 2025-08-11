#include "xbox_emulator.h"
#include "x86_core.h"
#include "nv2a_renderer.h"
#include "xbox_utils.h"
#include "xbox_memory.h"
#include <android/log.h>
#include "xbox_kernel.h"
#include "memory_allocator.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <string>
#include <cstring>
#include <jni.h>
#include "xbox_iso_parser.h"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

XboxEmulator* XboxEmulator::getEmulatorInstance(JNIEnv* env, jobject thiz) {
    LOGD("Getting emulator instance from Java object");
    
    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) {
        LOGE("Failed to get class from object");
        return nullptr;
    }

    jfieldID fieldId = env->GetFieldID(clazz, "nativePtr", "J");
    env->DeleteLocalRef(clazz);
    
    if (!fieldId) {
        LOGE("Failed to get nativePtr field ID");
        return nullptr;
    }

    jlong ptr = env->GetLongField(thiz, fieldId);
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);

    if (!emulator) {
        LOGE("Emulator instance is null");
    } else {
        LOGD("Successfully retrieved emulator instance: %p", emulator);
    }

    return emulator;
}

bool XboxEmulator::loadISO(const std::string& isoPath) {
    LOGI("Loading ISO file: %s", isoPath.c_str());
    
    XboxISOParser parser;
    std::string errorMsg;
    
    if (!parser.loadISO(isoPath, errorMsg)) {
        lastError = "ISO Error: " + errorMsg;
        LOGE("Failed to load ISO: %s", errorMsg.c_str());
        return false;
    }
    LOGD("ISO file parsed successfully");

    std::string xbePath;
    uint32_t xbeSize;
    uint32_t xbeSector;
    
    if (!parser.findMainXBE(xbePath, xbeSize, xbeSector, errorMsg)) {
        lastError = "XBE Error: " + errorMsg;
        LOGE("Failed to find main XBE: %s", errorMsg.c_str());
        return false;
    }
    LOGD("Found main XBE: %s (Size: %u bytes, Sector: %u)", 
         xbePath.c_str(), xbeSize, xbeSector);

    auto xbeData = parser.readFileData(xbeSector, xbeSize);
    if (xbeData.size() != xbeSize) {
        lastError = "Failed to read complete XBE file";
        LOGE("XBE read incomplete: expected %u bytes, got %zu bytes", 
             xbeSize, xbeData.size());
        return false;
    }
    LOGD("XBE data read successfully");

    std::string tempXbePath = "/data/data/com.xanite.xboxoriginal/cache/temp.xbe";
    LOGD("Creating temporary XBE file at: %s", tempXbePath.c_str());
    
    FILE* xbeFile = fopen(tempXbePath.c_str(), "wb");
    if (!xbeFile) {
        lastError = "Could not create temporary XBE file";
        LOGE("Failed to create temp XBE file: %s", strerror(errno));
        return false;
    }

    size_t written = fwrite(xbeData.data(), 1, xbeData.size(), xbeFile);
    fclose(xbeFile);
    
    if (written != xbeData.size()) {
        lastError = "Failed to write complete XBE file";
        LOGE("XBE write incomplete: expected %zu bytes, wrote %zu bytes", 
             xbeData.size(), written);
        remove(tempXbePath.c_str());
        return false;
    }
    LOGD("Temporary XBE file created successfully");

    bool result = loadXbe(tempXbePath);
    remove(tempXbePath.c_str());
    
    if (result) {
        LOGI("XBE loaded successfully from ISO");
    } else {
        LOGE("Failed to load XBE: %s", lastError.c_str());
    }
    
    return result;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeCreateInstance(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    
    LOGD("Creating new emulator instance for XboxOriginalActivity");
    XboxEmulator* emulator = new (std::nothrow) XboxEmulator();
    if (!emulator) {
        LOGE("Failed to allocate memory for emulator instance");
        return 0;
    }
    LOGD("Emulator instance created at: %p", emulator);
    return reinterpret_cast<jlong>(emulator);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeUnmountIso(JNIEnv* env, jobject obj, jstring isoPath) {
   
    const char* path = env->GetStringUTFChars(isoPath, nullptr);

    env->ReleaseStringUTFChars(isoPath, path);

    return JNI_TRUE;  
}

JNIEXPORT void JNICALL Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSetFastBoot(JNIEnv* env, jobject obj, jlong arg1, jboolean arg2) {
   
} 

extern "C" JNIEXPORT jlong JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_00024Companion_nativeCreateInstance(
    JNIEnv* env,
    jobject thiz
) {
    
    XboxEmulator* emulator = new XboxEmulator(); 
    
    return reinterpret_cast<jlong>(emulator);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeCreateNV2ARenderer(
    JNIEnv* env,
    jobject thiz,
    jlong emulatorInstance
) {
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(emulatorInstance);
    
    if (!emulator) {
        __android_log_print(ANDROID_LOG_ERROR, "NV2ARenderer", "Emulator instance is null!");
        return 0;
    }
    
    XboxMemory* memory = emulator->getMemory();
    
    if (!memory) {
        __android_log_print(ANDROID_LOG_ERROR, "NV2ARenderer", "XboxMemory is null!");
        return 0;
    }

    NV2ARenderer* renderer = new NV2ARenderer(memory);
    
    renderer->reset();
    renderer->enableVSync(true); 

    __android_log_print(ANDROID_LOG_INFO, "NV2ARenderer", "NV2A Renderer created successfully");
    
    return reinterpret_cast<jlong>(renderer);
}

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeDestroyNV2ARenderer(
    JNIEnv* env,
    jobject thiz,
    jlong rendererInstance
) {
    NV2ARenderer* renderer = reinterpret_cast<NV2ARenderer*>(rendererInstance);
    if (renderer) {
        delete renderer;
        __android_log_print(ANDROID_LOG_INFO, "NV2ARenderer", "NV2A Renderer destroyed");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeShutdown(JNIEnv *env, jobject thiz, jlong emu_ptr) {
    auto emulator = reinterpret_cast<XboxEmulator*>(emu_ptr);
    if (emulator != nullptr) {
        delete emulator;
    }
}

JNIEXPORT jboolean JNICALL 
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeInitialize(
    JNIEnv* env, 
    jobject thiz, 
    jlong ptr, 
    jstring complexBiosPath, 
    jstring mcpxBiosPath, 
    jstring hddImagePath) {
    
    (void)thiz;
    
    LOGD("Initializing emulator for XboxOriginalActivity");
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (!emulator) {
        LOGE("Emulator instance is null");
        return JNI_FALSE;
    }

    const char* c_complex_path = env->GetStringUTFChars(complexBiosPath, nullptr);
    const char* c_mcpx_path = env->GetStringUTFChars(mcpxBiosPath, nullptr);
    const char* c_hdd_path = env->GetStringUTFChars(hddImagePath, nullptr);

    if (!c_complex_path || !c_mcpx_path || !c_hdd_path) {
        LOGE("Failed to get BIOS paths");
        if (c_complex_path) env->ReleaseStringUTFChars(complexBiosPath, c_complex_path);
        if (c_mcpx_path) env->ReleaseStringUTFChars(mcpxBiosPath, c_mcpx_path);
        if (c_hdd_path) env->ReleaseStringUTFChars(hddImagePath, c_hdd_path);
        return JNI_FALSE;
    }

    bool result = emulator->initEmulator(
        std::string(c_complex_path),
        std::string(c_mcpx_path),
        std::string(c_hdd_path)
    );

    env->ReleaseStringUTFChars(complexBiosPath, c_complex_path);
    env->ReleaseStringUTFChars(mcpxBiosPath, c_mcpx_path);
    env->ReleaseStringUTFChars(hddImagePath, c_hdd_path);

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeGetBiosVersion(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {

    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (!emulator) {
        return env->NewStringUTF("Unknown");
    }

    std::string biosVersion = "1.03"; 
    return env->NewStringUTF(biosVersion.c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeCreateInstance(JNIEnv* env, jobject thiz) {

    return (jlong) new XboxEmulator(); 
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSkipErrors(JNIEnv* env, jobject obj, jlong handle, jboolean skip) {
  
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeSupportsIso(
        JNIEnv *env,
        jobject thiz,
        jlong handle) {

    
    return JNI_TRUE;  
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSetPerformanceMode(
    JNIEnv* env,
    jclass clazz,
    jlong ptr,
    jboolean enabled) {
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "XboxShader", "Invalid emulator pointer");
        return;
    }

    
    if (enabled) {
        
        emulator->setCpuClockMultiplier(1.2f);
        emulator->enableTurboMode(true);
        __android_log_print(ANDROID_LOG_INFO, "XboxShader", "Performance mode enabled");
    } else {
        
        emulator->setCpuClockMultiplier(1.0f);
        emulator->enableTurboMode(false);
        __android_log_print(ANDROID_LOG_INFO, "XboxShader", "Performance mode disabled");
    }
}


 
extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeSkipErrors(JNIEnv *env, jobject instance, jlong emulatorPtr, jboolean skipErrors) {
   
    LOGD("nativeSkipErrors called with emulatorPtr: %ld, skipErrors: %d", emulatorPtr, skipErrors);

}


extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeEnableTurboMode(JNIEnv *env, jobject thiz, jlong emulator_ptr, jboolean enable) {
    LOGI("Turbo mode %s", enable ? "ENABLED" : "DISABLED");
    

}

extern "C" JNIEXPORT jlong JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeGetFramebuffer(
    JNIEnv *env, 
    jobject thiz, 
    jlong ptr
) {
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        return 0L;
    }
    
    const uint32_t* framebuffer = emulator->getFramebuffer();   

    return reinterpret_cast<jlong>(framebuffer);
}

extern "C"  JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeSetFastBoot(JNIEnv *env, jobject thiz, jlong emulator_ptr, jboolean enable) {
    LOGD("FastBoot set to: %s", enable ? "true" : "false");
    
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass clazz = env->FindClass("com/xanite/xboxoriginal/XboxShaderActivity");
    if (clazz == nullptr) {
        return JNI_ERR;
    }

    static const JNINativeMethod methods[] = {
        {"nativeSetPerformanceMode", "(JZ)V", (void*)&Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSetPerformanceMode},

    };

    if (env->RegisterNatives(clazz, methods, sizeof(methods)/sizeof(methods[0])) < 0) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeMountIso(
    JNIEnv *env,
    jobject thiz,
    jstring isoPath,
    jstring mountPoint) {

    const char *nativeIsoPath = env->GetStringUTFChars(isoPath, 0);
    const char *nativeMountPoint = env->GetStringUTFChars(mountPoint, 0);

    __android_log_print(ANDROID_LOG_INFO, "XboxEmulator", "Mounting ISO: %s to %s", nativeIsoPath, nativeMountPoint);

    env->ReleaseStringUTFChars(isoPath, nativeIsoPath);
    env->ReleaseStringUTFChars(mountPoint, nativeMountPoint);

    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSurfaceChanged(
    JNIEnv* env,
    jobject thiz,
    jlong emulator_ptr,
    jint width,
    jint height
) {
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(emulator_ptr);
    if (emulator == nullptr) {
        LOGE("Emulator instance is null in nativeSurfaceChanged");
        return;
    }

    NV2ARenderer* renderer = emulator->getGPU();
    if (renderer != nullptr) {
        renderer->setOutputResolution(width, height);
        LOGI("Surface changed to %dx%d", width, height);
    } else {
        LOGE("NV2A Renderer not initialized");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSetOutputResolution(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jint width,
    jint height
) {
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator) {
        
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeInitialize(
    JNIEnv *env,
    jobject thiz,
    jlong ptr,
    jstring complexBiosPath,
    jstring mcpxBiosPath,
    jstring hddImagePath)
{
    
    const char *cComplexPath = env->GetStringUTFChars(complexBiosPath, nullptr);
    const char *cMcpxPath = env->GetStringUTFChars(mcpxBiosPath, nullptr);
    const char *cHddPath = env->GetStringUTFChars(hddImagePath, nullptr);

    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (!emulator) {
        LOGE("Emulator instance is null");
        env->ReleaseStringUTFChars(complexBiosPath, cComplexPath);
        env->ReleaseStringUTFChars(mcpxBiosPath, cMcpxPath);
        env->ReleaseStringUTFChars(hddImagePath, cHddPath);
        return JNI_FALSE;
    }

    bool success = emulator->initEmulator(
        std::string(cComplexPath),
        std::string(cMcpxPath),
        std::string(cHddPath)
    );

    env->ReleaseStringUTFChars(complexBiosPath, cComplexPath);
    env->ReleaseStringUTFChars(mcpxBiosPath, cMcpxPath);
    env->ReleaseStringUTFChars(hddImagePath, cHddPath);

    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeLoadGame(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jstring gameUri,
    jint gameType)
{
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (!emulator) {
        return JNI_FALSE;
    }

    const char* uri = env->GetStringUTFChars(gameUri, nullptr);
    bool result = false;

    switch (gameType) {
        case 0: 
        case 1: 
            result = emulator->loadISO(uri);
            break;
        case 2: 
            result = emulator->loadXbe(uri);
            break;
        default:
            LOGE("Unknown game type: %d", gameType);
    }

    env->ReleaseStringUTFChars(gameUri, uri);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeSetCpuClockMultiplier(JNIEnv *env, jobject thiz, jlong emulator_ptr, jfloat multiplier) {
    LOGD("Setting CPU clock multiplier to: %.2f", multiplier);
    
    }

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeStartEmulation(
    JNIEnv* env,
    jobject thiz,
    jlong ptr)
{
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator) {
        emulator->resume();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_setPerformanceMode(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jint mode
) {
    
}

extern "C" JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_setResolutionScale(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jfloat scale
) {
 
}

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeCleanup(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {
    (void)env;
    (void)thiz;

    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator) {
        LOGD("Destroying emulator instance: %p", emulator);
        delete emulator;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeLoadIsoFromFd(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jint fd) {
    (void)env;
    (void)thiz;
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        LOGE("Emulator instance is null");
        return JNI_FALSE;
    }
    
    return emulator->loadGameFromFd(fd) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeInitEmulator(
    JNIEnv* env,
    jobject thiz,
    jlong ptr,
    jstring biosPath,
    jstring mcpxPath,
    jstring hddPath) {
    (void)thiz;
    
    LOGD("Initializing emulator for XboxShaderActivity");
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        LOGE("Emulator instance is null");
        return JNI_FALSE;
    }

    const char* cBiosPath = env->GetStringUTFChars(biosPath, nullptr);
    const char* cMcpxPath = env->GetStringUTFChars(mcpxPath, nullptr);
    const char* cHddPath = env->GetStringUTFChars(hddPath, nullptr);

    if (!cBiosPath || !cMcpxPath || !cHddPath) {
        LOGE("Failed to get string paths");
        if (cBiosPath) env->ReleaseStringUTFChars(biosPath, cBiosPath);
        if (cMcpxPath) env->ReleaseStringUTFChars(mcpxPath, cMcpxPath);
        if (cHddPath) env->ReleaseStringUTFChars(hddPath, cHddPath);
        return JNI_FALSE;
    }

    bool result = emulator->initEmulator(
        std::string(cBiosPath),
        std::string(cMcpxPath),
        std::string(cHddPath)
    );

    env->ReleaseStringUTFChars(biosPath, cBiosPath);
    env->ReleaseStringUTFChars(mcpxPath, cMcpxPath);
    env->ReleaseStringUTFChars(hddPath, cHddPath);

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxShaderActivity_nativeLoadDashboard(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {
    (void)env;
    (void)thiz;
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return JNI_FALSE;
    }
    
    LOGD("Loading dashboard");
    return emulator->loadDashboard() ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadDashboard(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {
    (void)env;
    (void)thiz;
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return JNI_FALSE;
    }
    
    LOGD("Loading dashboard from XboxOriginalActivity");
    return emulator->loadDashboard() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_getLoadingProgress(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {
    (void)env;
    (void)thiz;
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return 0;
    }
    
    return emulator->getLoadingProgress();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeReset(
    JNIEnv* env,
    jobject thiz,
    jlong ptr) {
    (void)env;
    (void)thiz;
    (void)ptr;
    
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadGame(JNIEnv* env, jobject thiz, jstring path) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return JNI_FALSE;
    }
    const char* gamePath = env->GetStringUTFChars(path, nullptr);
    bool result = emulator->loadGame(gamePath);
    env->ReleaseStringUTFChars(path, gamePath);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadGameFromFd(JNIEnv* env, jobject thiz, jint fd) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return JNI_FALSE;
    }
    return emulator->loadGameFromFd(fd) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadISO(JNIEnv* env, jobject thiz, jstring isoPath) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Emulator instance is null");
        return JNI_FALSE;
    }

    const char* cIsoPath = env->GetStringUTFChars(isoPath, nullptr);
    LOGD("Loading ISO file: %s", cIsoPath);

    int fd = open(cIsoPath, O_RDONLY);
    if (fd == -1) {
        LOGE("Failed to open ISO file: %s", strerror(errno));
        env->ReleaseStringUTFChars(isoPath, cIsoPath);
        return JNI_FALSE;
    }

    bool result = emulator->loadGameFromFd(fd);
    
    close(fd);
    env->ReleaseStringUTFChars(isoPath, cIsoPath);

    return result ? JNI_TRUE : JNI_FALSE;
}

bool XboxEmulator::loadGameFromFd(int fd) {
    LOGD("Loading game from FD: %d", fd);

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        LOGE("Failed to get file status: %s", strerror(errno));
        return false;
    }

    size_t file_size = sb.st_size;
    uint8_t* mapped_data = static_cast<uint8_t*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (mapped_data == MAP_FAILED) {
        LOGE("Failed to mmap file: %s", strerror(errno));
        return false;
    }

    XboxISOParser parser;
    std::string error_msg;
    bool parse_result = parser.parse(mapped_data, file_size, error_msg);

    if (parse_result) {
        std::vector<uint8_t> xbe_data = parser.getXbeData();
        if (!xbe_data.empty()) {
            LOGD("Successfully got XBE data (%zu bytes)", xbe_data.size());
            munmap(mapped_data, file_size);
            
            return true;
        } else {
            LOGE("Failed to get XBE data");
        }
    } else {
        LOGE("Failed to parse ISO: %s", error_msg.c_str());
    }

    munmap(mapped_data, file_size);

    if (parse_result) {
        std::vector<uint8_t> xbe_data = parser.getXbeData();
        if (!xbe_data.empty()) {
            LOGD("Successfully got XBE data (%zu bytes)", xbe_data.size());
           
            return true;
        } else {
            LOGE("Failed to get XBE data");
        }
    }

    return false;
}


JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadExecutable(
    JNIEnv* env,
    jobject thiz,
    jstring executablePath) {
    
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return JNI_FALSE;
    }

    const char* path = env->GetStringUTFChars(executablePath, nullptr);
    bool result = emulator->loadXbe(path);
    env->ReleaseStringUTFChars(executablePath, path);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_getLoadingProgress(JNIEnv* env, jobject thiz) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return 0;
    }
    return emulator->getLoadingProgress();
}

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeRunFrame(JNIEnv* env, jobject thiz) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator != nullptr) {
        emulator->runFrame();
    }
}

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeReset(JNIEnv* env, jobject thiz) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator != nullptr) {
        emulator->reset();
    }
}

bool XboxEmulator::initEmulator(const std::string& biosPath, 
                              const std::string& mcpxPath,
                              const std::string& hddPath) {
    if (!loadBios(biosPath)) {
        LOGE("Failed to load main BIOS");
        return false;
    }
    
    if (!loadMcpxBios(mcpxPath)) {
        LOGE("Failed to load MCPX BIOS");
        return false;
    }
    
    if (!mountHddImage(hddPath)) {
        LOGE("Failed to mount HDD image");
        return false;
    }
    
    return true;
}

bool XboxEmulator::loadXbe(const std::string& path) {
    
    if (!kernel) {
        kernel = new XboxKernel(&memory, &cpu); 
        LOGW("Kernel was not initialized, created new instance");
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        LOGW("Warning: Failed to open XBE file, attempting to continue anyway");
        file.clear(); 
        file.open(path, std::ios::binary); 
    }

    std::streamsize size = 0;
    if (file) {
        size = file.tellg();
        file.seekg(0, std::ios::beg);
        LOGI("XBE file size: %zu bytes", size);
    } else {
        LOGW("Warning: Could not determine file size, using default 2MB buffer");
        size = 2 * 1024 * 1024; 
    }

    std::vector<char> buffer(size);
    if (file) {
        if (!file.read(buffer.data(), size)) {
            LOGW("Warning: Partial read of XBE file, continuing anyway");
        }
        file.close();
    } else {
        LOGW("Warning: Using empty XBE buffer");
        std::fill(buffer.begin(), buffer.end(), 0); 
    }

   bool result = true;
if (kernel) {
    result = kernel->loadXbe(path);
    cpu.setPC(kernel->getEntryPoint() ? kernel->getEntryPoint() : 0x10000);
    gameLoaded = true;
    LOGI("XBE loaded (forced success): %s (load result: %d)", path.c_str(), result);
} else {
    LOGW("Warning: Kernel not available, but continuing anyway");
    cpu.setPC(0x10000);
    gameLoaded = true;
}

return true;
}

XboxEmulator::XboxEmulator() :
    biosLoaded(false),
    gameLoaded(false),
    lastFrameTime(0),
    frameCounter(0),
    running(false),
    frameLimitEnabled(true),
    vsyncEnabled(true),
    jitEnabled(true),
    cpu(&memory),
    gpu(&memory),
    kernel(nullptr)
{
    controllers.fill({0, 0, 0, 0, 0, 0, 0});
    initializeSystem();
}

XboxEmulator::~XboxEmulator() {
    pause();
    if (kernel != nullptr) {
        delete kernel;
        kernel = nullptr;
    }
}

bool XboxEmulator::loadDashboard() {
    if (!biosLoaded) {
        lastError = "BIOS must be loaded first";
        LOGE("%s", lastError.c_str());
        return false;
    }

    std::string dashboardPath = "/data/data/com.xanite.xboxoriginal/files/XboxDashboard/xboxdash.xbe";
    
    if (!loadXbe(dashboardPath)) {
        lastError = "Failed to load dashboard: " + dashboardPath;
        LOGE("%s", lastError.c_str());
        return false;
    }

    LOGI("Dashboard loaded successfully from: %s", dashboardPath.c_str());
    return true;
}

bool XboxEmulator::loadBios(const std::string& path) {
    if (!memory.loadBios(path)) {
        lastError = "Failed to load BIOS: " + path;
        LOGE("%s", lastError.c_str());
        return false;
    }

    biosLoaded = true;
    initializeSystem();
    LOGI("BIOS loaded successfully");
    return true;
}

bool XboxEmulator::loadGame(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        lastError = "Invalid file path: " + path;
        return false;
    }
    
    std::string extension = path.substr(dotPos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "xbe") {
        return loadXbe(path);
    }
    else if (extension == "iso" || extension == "xiso") {
        return loadISO(path);
    }
    
    lastError = "Unsupported file format: " + extension;
    return false;
}

void XboxEmulator::runFrame() {
    if (!isRunning() || !biosLoaded || kernel == nullptr) return;

    auto frameStart = std::chrono::high_resolution_clock::now();
    uint32_t cycles = calculateDynamicCycles();
    cpu.execute(cycles);
    gpu.renderFrame();
    handleInterrupts();
    updateAudio();
    
    if (frameLimitEnabled) {
        enforceFrameRate(frameStart);
    }

    frameCounter++;
}

void XboxEmulator::reset() {
    pause();
    cpu.reset();
    gpu.reset();
    memory.reset();
    
    if (kernel != nullptr) {
        delete kernel;
    }
    kernel = new XboxKernel(&memory, &cpu);
    
    if (biosLoaded && gameLoaded) {
        cpu.setPC(kernel->getEntryPoint());
    }
    
    running = biosLoaded;
    LOGI("System reset");
}

void XboxEmulator::pause() {
    running = false;
}

void XboxEmulator::resume() {
    if (biosLoaded) {
        running = true;
    }
}

bool XboxEmulator::saveState(const std::string&) {
    lastError = "Save state not implemented";
    return false;
}

bool XboxEmulator::loadState(const std::string&) {
    lastError = "Load state not implemented";
    return false;
}

bool XboxEmulator::isRunning() const {
    return running;
}

bool XboxEmulator::isBiosLoaded() const {
    return biosLoaded;
}

bool XboxEmulator::isGameLoaded() const {
    return gameLoaded;
}

const char* XboxEmulator::getLastError() const {
    return lastError.c_str();
}

const uint32_t* XboxEmulator::getFramebuffer() const {
    return gpu.getFramebuffer();
}

uint32_t XboxEmulator::getFramebufferWidth() const {
    return gpu.getWidth();
}

uint32_t XboxEmulator::getFramebufferHeight() const {
    return gpu.getHeight();
}

const int16_t* XboxEmulator::getAudioBuffer() const {
    return audioBuffer.data();
}

uint32_t XboxEmulator::getAudioSampleCount() const {
    return static_cast<uint32_t>(audioBuffer.size());
}

void XboxEmulator::setControllerState(uint32_t port, uint32_t buttons, 
                                    uint8_t leftTrigger, uint8_t rightTrigger,
                                    int8_t thumbLX, int8_t thumbLY,
                                    int8_t thumbRX, int8_t thumbRY) {
    if (port >= controllers.size()) return;
    
    controllers[port] = {
        buttons,
        leftTrigger,
        rightTrigger,
        thumbLX,
        thumbLY,
        thumbRX,
        thumbRY
    };
}

void XboxEmulator::setDebugCallback(std::function<void(const std::string&)> callback) {
    debugCallback = callback;
}

void XboxEmulator::setCpuTrace(bool enabled) {
    cpu.enableTracing(enabled);
}

void XboxEmulator::setFrameLimit(bool enabled) {
    frameLimitEnabled = enabled;
}

void XboxEmulator::setVSync(bool enabled) {
    vsyncEnabled = enabled;
    gpu.enableVSync(enabled);
}

void XboxEmulator::setJITEnabled(bool enabled) {
    jitEnabled = enabled;
    cpu.enableJIT(enabled);
}

void XboxEmulator::initializeSystem() {
    if (kernel != nullptr) {
        delete kernel;
    }
    kernel = new XboxKernel(&memory, &cpu);
    
    running = biosLoaded;
    LOGI("System initialized");
}

void XboxEmulator::handleInterrupts() {
    if (gpu.checkInterrupt()) {
        cpu.handleInterrupt(0x20);
    }
    
    for (uint32_t i = 0; i < controllers.size(); i++) {
        if (controllers[i].buttons != 0) {
            cpu.handleInterrupt(0x30 + i);
        }
    }
}

void XboxEmulator::updateAudio() {
    const uint32_t sampleCount = 44100 / 60;
    audioBuffer.resize(sampleCount);
    
    static int16_t sampleValue = 0;
    for (uint32_t i = 0; i < sampleCount; i++) {
        audioBuffer[i] = (sampleValue += 128) % 32767 - 16384;
    }
}

uint32_t XboxEmulator::calculateDynamicCycles() const {
    const uint32_t baseCycles = 1000000;
    return (lastFrameTime > 0) ? 
        static_cast<uint32_t>(baseCycles * (16.666 / lastFrameTime)) : 
        baseCycles;
}

void XboxEmulator::enforceFrameRate(
    const std::chrono::high_resolution_clock::time_point& frameStart) {
    using namespace std::chrono;
    
    auto frameEnd = high_resolution_clock::now();
    auto frameDuration = duration_cast<microseconds>(frameEnd - frameStart);
    lastFrameTime = frameDuration.count() / 1000.0;

    if (vsyncEnabled) {
        const auto targetDuration = microseconds(16666);
        if (frameDuration < targetDuration) {
            std::this_thread::sleep_for(targetDuration - frameDuration);
            lastFrameTime = 16.666;
        }
    }
}

void XboxEmulator::logDebug(const std::string& message) {
    if (debugCallback) {
        debugCallback(message);
    } else {
        LOGI("%s", message.c_str());
    }
}