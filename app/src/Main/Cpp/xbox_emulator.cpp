#include "xbox_emulator.h"
#include "xbox_utils.h"
#include <android/log.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

#define LOG_TAG "XboxEmulator"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

XboxEmulator::XboxEmulator() :
    biosLoaded(false),
    gameLoaded(false),
    running(false),
    frameLimitEnabled(true),
    vsyncEnabled(true),
    jitEnabled(true),
    lastFrameTime(0),
    frameCounter(0),
    cpu(&memory),
    gpu(&memory),
    kernel(&memory, &cpu)
{
    controllers.fill({0, 0, 0, 0, 0, 0, 0});
    initializeSystem();
}

XboxEmulator::~XboxEmulator() {
    pause();
}

bool XboxEmulator::loadBios(const std::string& path) {
    auto biosData = XboxUtils::readFile(path);
    if (biosData.empty()) {
        lastError = "Failed to read BIOS file";
        return false;
    }

    if (!memory.loadBios(biosData)) {
        lastError = "Invalid BIOS image";
        return false;
    }

    biosLoaded = true;
    initializeSystem();
    return true;
}

bool XboxEmulator::loadGame(const std::string& path) {
    if (!biosLoaded) {
        lastError = "BIOS must be loaded first";
        return false;
    }

    if (!kernel.loadXbe(path)) {
        lastError = "Failed to load XBE file";
        return false;
    }

    cpu.setPC(kernel.getEntryPoint());
    gameLoaded = true;
    return true;
}

void XboxEmulator::runFrame() {
    if (!isRunning() || !biosLoaded) return;

    auto frameStart = std::chrono::high_resolution_clock::now();

    
    uint32_t cycles = calculateDynamicCycles();
    cpu.execute(cycles);

    
    gpu.processCommands();
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
    kernel.reset();
    memory.reset();
    initializeSystem();
    
    if (biosLoaded && gameLoaded) {
        cpu.setPC(kernel.getEntryPoint());
    }
    
    running = biosLoaded;
}

void XboxEmulator::pause() {
    running = false;
}

void XboxEmulator::resume() {
    if (biosLoaded) {
        running = true;
    }
}

bool XboxEmulator::saveState(const std::string& path) {
    try {
        std::vector<uint8_t> state;
        
        
        auto cpuState = cpu.saveState();
        state.insert(state.end(), cpuState.begin(), cpuState.end());
        
        
        auto memState = memory.saveState();
        state.insert(state.end(), memState.begin(), memState.end());
        
        
        auto gpuState = gpu.saveState();
        state.insert(state.end(), gpuState.begin(), gpuState.end());
        
        return XboxUtils::writeFile(path, state);
    } catch (const std::exception& e) {
        lastError = e.what();
        return false;
    }
}

bool XboxEmulator::loadState(const std::string& path) {
    try {
        auto state = XboxUtils::readFile(path);
        if (state.empty()) {
            lastError = "Empty state file";
            return false;
        }
        
        size_t offset = 0;
        
        
        offset += cpu.loadState(state.data() + offset, state.size() - offset);
        
        
        offset += memory.loadState(state.data() + offset, state.size() - offset);
        
        
        offset += gpu.loadState(state.data() + offset, state.size() - offset);
        
        return true;
    } catch (const std::exception& e) {
        lastError = e.what();
        return false;
    }
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
    return gpu.getFramebufferWidth();
}

uint32_t XboxEmulator::getFramebufferHeight() const {
    return gpu.getFramebufferHeight();
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
    cpu.setDebugCallback(callback);
    gpu.setDebugCallback(callback);
    kernel.setDebugCallback(callback);
}

void XboxEmulator::setCpuTrace(bool enabled) {
    cpu.setTracingEnabled(enabled);
}

void XboxEmulator::setFrameLimit(bool enabled) {
    frameLimitEnabled = enabled;
}

void XboxEmulator::setVSync(bool enabled) {
    vsyncEnabled = enabled;
}

void XboxEmulator::setJITEnabled(bool enabled) {
    jitEnabled = enabled;
    cpu.setJITEnabled(enabled);
}

void XboxEmulator::initializeSystem() {
    
    memory.mapPhysicalMemory();
    
    kernel.initialize();
    
    cpu.initialize();
    
    gpu.initialize();
    
    running = biosLoaded;
}

void XboxEmulator::handleInterrupts() {
    if (gpu.hasInterrupt()) {
        cpu.raiseInterrupt(0x20); 
    }
    
    for (uint32_t i = 0; i < controllers.size(); i++) {
        if (controllers[i].buttons != 0) {
            cpu.raiseInterrupt(0x30 + i); 
        }
    }
}

void XboxEmulator::updateAudio() {
    const uint32_t sampleCount = 44100 / 60; 
    audioBuffer.resize(sampleCount);
    
    for (uint32_t i = 0; i < sampleCount; i++) {
        
        audioBuffer[i] = ((i % 100) < 50) ? 16384 : -16384;
    }
}

uint32_t XboxEmulator::calculateDynamicCycles() const {
    const uint32_t baseCycles = 1000000; 
    
    if (lastFrameTime == 0) return baseCycles;
    
    double ratio = (16.666 / lastFrameTime);
    uint32_t adjusted = static_cast<uint32_t>(baseCycles * ratio);
    
    return std::clamp(adjusted, baseCycles / 2, baseCycles * 2);
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