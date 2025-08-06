#ifndef XBOX_EMULATOR_H
#define XBOX_EMULATOR_H

#include "xbox_iso_parser.h"
#include <jni.h>
#include "xbox_memory.h"
#include "x86_core.h"
#include "nv2a_renderer.h"
#include "xbox_kernel.h"
#include "xbox_utils.h"
#include <functional>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <android/log.h>

#define LOG_TAG "XboxEmulatorNative"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class XboxEmulator {
public:
    XboxEmulator();
    ~XboxEmulator();

    bool initEmulator(const std::string& biosPath, 
                     const std::string& mcpxPath,
                     const std::string& hddPath);
    bool loadISO(const std::string& isoPath);
    bool loadGameFromFd(int fd);
    bool loadXbeFromFd(int fd);                  
    bool loadBios(const std::string& path);
    bool loadGame(const std::string& path);
    bool loadXbe(const std::string& path);
    void runFrame();
    void reset();
    void pause();
    void resume();
    bool saveState(const std::string& path);
    bool loadState(const std::string& path);
    bool loadDashboard();
    static XboxEmulator* getEmulatorInstance(JNIEnv* env, jobject thiz);
    
    bool isRunning() const;
    bool isBiosLoaded() const;
    bool isGameLoaded() const;
    const char* getLastError() const;

    const uint32_t* getFramebuffer() const;
    uint32_t getFramebufferWidth() const;
    uint32_t getFramebufferHeight() const;

    const int16_t* getAudioBuffer() const;
    uint32_t getAudioSampleCount() const;

    void setControllerState(uint32_t port, uint32_t buttons, 
                          uint8_t leftTrigger, uint8_t rightTrigger,
                          int8_t thumbLX, int8_t thumbLY,
                          int8_t thumbRX, int8_t thumbRY);

    void setDebugCallback(std::function<void(const std::string&)> callback);
    void setCpuTrace(bool enabled);
    void setFrameLimit(bool enabled);
    void setVSync(bool enabled);
    void setJITEnabled(bool enabled);

    bool loadMcpxBios(const std::string& path) {
        LOGI("Loading MCPX BIOS from: %s", path.c_str());
        return true;
    }
    
    bool mountHddImage(const std::string& path) {
        LOGI("Mounting HDD image from: %s", path.c_str());
        return true;
    }

    bool isReady() const {
        return biosLoaded && kernel != nullptr;
    }

private:
    void initializeSystem();
    void handleInterrupts();
    void updateAudio();
    uint32_t calculateDynamicCycles() const;
    void enforceFrameRate(const std::chrono::high_resolution_clock::time_point& frameStart);
    void logDebug(const std::string& message);

    bool biosLoaded = false;
    bool gameLoaded = false;
    double lastFrameTime = 0.0;
    uint32_t frameCounter = 0;
    std::atomic<bool> running{false};
    bool frameLimitEnabled = true;
    bool vsyncEnabled = true;
    bool jitEnabled = false;

    XboxMemory memory;
    X86Core cpu;
    NV2ARenderer gpu;
    XboxKernel* kernel = nullptr;

    struct ControllerState {
        uint32_t buttons = 0;
        uint8_t leftTrigger = 0;
        uint8_t rightTrigger = 0;
        int8_t thumbLX = 0;
        int8_t thumbLY = 0;
        int8_t thumbRX = 0;
        int8_t thumbRY = 0;
    };
    std::array<ControllerState, 4> controllers;
     
    std::string mountIsoAndFindXbe(const std::string& isoPath);

    std::vector<int16_t> audioBuffer;
    std::string lastError;
    std::function<void(const std::string&)> debugCallback;
};

#endif // XBOX_EMULATOR_H