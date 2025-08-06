#include "xbox_emulator.h"
#include "xbox_utils.h"
#include <android/log.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <jni.h>
#include "xbox_iso_parser.h"
#include <cstdio>
#include <sys/stat.h>

#undef LOG_TAG
#define LOG_TAG "XboxEmulatorNative"

XboxEmulator* XboxEmulator::getEmulatorInstance(JNIEnv* env, jobject thiz) {
    LOGD("Getting emulator instance from Java object");
    
    jclass clazz = env->FindClass("com/xanite/xboxoriginal/XboxOriginalActivity");
    if (clazz == nullptr) {
        LOGE("Failed to find XboxOriginalActivity class");
        return nullptr;
    }

    jfieldID fieldId = env->GetFieldID(clazz, "nativePtr", "J");
    env->DeleteLocalRef(clazz);
    
    if (fieldId == nullptr) {
        LOGE("Failed to get nativePtr field ID");
        return nullptr;
    }

    jlong ptr = env->GetLongField(thiz, fieldId);
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    
    if (emulator == nullptr) {
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
Java_com_xanite_xboxoriginal_XboxOriginalActivity_createEmulatorInstance(JNIEnv* env, jobject thiz) {
    (void)env;  // Mark as unused to suppress warning
    (void)thiz; // Mark as unused to suppress warning
    
    LOGD("Creating new emulator instance");
    XboxEmulator* emulator = new XboxEmulator();
    LOGD("Emulator instance created at: %p", emulator);
    return reinterpret_cast<jlong>(emulator);
}

JNIEXPORT void JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeCleanup(JNIEnv* env, jobject thiz, jlong ptr) {
    (void)env;  // Mark as unused to suppress warning
    (void)thiz; // Mark as unused to suppress warning
    
    XboxEmulator* emulator = reinterpret_cast<XboxEmulator*>(ptr);
    if (emulator != nullptr) {
        delete emulator;
        LOGD("Emulator instance destroyed: %p", emulator);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeInitEmulator(
    JNIEnv* env,
    jobject thiz,
    jstring biosPath,
    jstring mcpxPath,
    jstring hddPath) {
    
    LOGD("Initializing emulator (OriginalActivity)");
    
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Emulator instance is null");
        return JNI_FALSE;
    }

    const char* cBiosPath = env->GetStringUTFChars(biosPath, nullptr);
    const char* cMcpxPath = env->GetStringUTFChars(mcpxPath, nullptr);
    const char* cHddPath = env->GetStringUTFChars(hddPath, nullptr);

    LOGD("Initializing with paths:");
    LOGD(" - BIOS: %s", cBiosPath);
    LOGD(" - MCPX: %s", cMcpxPath);
    LOGD(" - HDD: %s", cHddPath);

    bool result = emulator->initEmulator(cBiosPath, cMcpxPath, cHddPath);

    env->ReleaseStringUTFChars(biosPath, cBiosPath);
    env->ReleaseStringUTFChars(mcpxPath, cMcpxPath);
    env->ReleaseStringUTFChars(hddPath, cHddPath);

    LOGD("Initialization result: %s", result ? "SUCCESS" : "FAILED");
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadDashboard(JNIEnv* env, jobject thiz) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return false;
    }
    return emulator->loadDashboard();
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadGame(JNIEnv* env, jobject thiz, jstring path) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return false;
    }
    const char* gamePath = env->GetStringUTFChars(path, nullptr);
    bool result = emulator->loadGame(gamePath);
    env->ReleaseStringUTFChars(path, gamePath);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadGameFromFd(JNIEnv* env, jobject thiz, jint fd) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return false;
    }
    
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadISO(JNIEnv* env, jobject thiz, jstring isoPath) {
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (emulator == nullptr) {
        LOGE("Failed to get emulator instance");
        return false;
    }
    const char* cIsoPath = env->GetStringUTFChars(isoPath, nullptr);
    bool result = emulator->loadISO(cIsoPath);
    env->ReleaseStringUTFChars(isoPath, cIsoPath);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_xanite_xboxoriginal_XboxOriginalActivity_nativeLoadExecutable(
    JNIEnv* env,
    jobject thiz,
    jstring executablePath) {
    
    XboxEmulator* emulator = XboxEmulator::getEmulatorInstance(env, thiz);
    if (!emulator) return false;

    const char* path = env->GetStringUTFChars(executablePath, nullptr);
    bool result = emulator->loadXbe(path);
    env->ReleaseStringUTFChars(executablePath, path);
    
    return result;
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

} // extern "C"

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
        LOGE("Kernel not initialized");
        lastError = "Kernel not initialized";
        return false;
    }
    
    if (!kernel->loadXbe(path)) {
        lastError = "Failed to load XBE: " + path;
        LOGE("%s", lastError.c_str());
        return false;
    }
    
    cpu.setPC(kernel->getEntryPoint());
    gameLoaded = true;
    LOGI("XBE executable loaded successfully: %s", path.c_str());
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

    if (kernel == nullptr) {
        lastError = "Kernel not initialized";
        LOGE("%s", lastError.c_str());
        return false;
    }

    const std::string dashboardPath = "C:\\xboxdash.xbe";
    if (!loadXbe(dashboardPath)) {
        lastError = "Failed to load dashboard: " + dashboardPath;
        LOGE("%s", lastError.c_str());
        return false;
    }

    LOGI("Xbox dashboard loaded successfully");
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
