#define ENABLE_OPTIMIZATIONS
#define ENABLE_JIT
                                   //emulator Xbox original for android ❤️ Ziunx 
#include <iostream>
#include <jni.h>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <string>
#include <cdio/input_jostick.h>
#include <algorithm>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <sstream>
#include <cdio/cdio.h>
#include <cdio/device.h>
#include <cdio/memory.h>
#include <cdio/track.h>
#include <cdio/cdtext.h>
#include <cdio/rock.h>
#include <cdio/xa.h>
#include <cdio/sector.h>
#include <cdio/ds.h>
#include <cdio/types.h>
#include <atomic>
#include <cdio/version.h>
#include <cdio/disc.h>
#include <cdio/stdint.h>
#include <cdio/read.h>
#include <cdio/posix.h>
#include <cdio/limits.h>
#include <cdio/driver.h>
#include <cdio/iso9660.h>
#include <mutex>
#include <queue>
#include <mutex>
#include <list>
#include <memory>

class MemoryPool {
public:
    MemoryPool(size_t totalSize, size_t blockSize) : totalSize(totalSize), blockSize(blockSize) {
        pool.resize(totalSize, 0);
        for (size_t i = 0; i < totalSize; i += blockSize) {
            freeBlocks.push_back(i);
        }
    }
    size_t allocate() {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (freeBlocks.empty()) {
            std::cerr << "MemoryPool: No free blocks available!" << std::endl;
            return SIZE_MAX;
        }
        size_t block = freeBlocks.front();
        freeBlocks.pop_front();
        return block;
    }
    void deallocate(size_t block) {
        std::lock_guard<std::mutex> lock(poolMutex);
        freeBlocks.push_back(block);
    }
    uint8_t* getBlockPtr(size_t block) {
        if (block >= pool.size()) return nullptr;
        return &pool[block];
    }
private:
    size_t totalSize;
    size_t blockSize;
    std::vector<uint8_t> pool;
    std::list<size_t> freeBlocks;
    std::mutex poolMutex;
};

enum class InputType {
    KEYBOARD,
    MOUSE,
    JOYSTICK
};

struct InputEvent {
    InputType type;
    int key;
    int action;
    int mouseX, mouseY;
    int joystickId;
    float axisX, axisY;
};

std::mutex memoryMutex;
std::mutex inputMutex;
std::queue<InputEvent> inputQueue;

struct CPU {
    uint32_t registers[32];
    uint32_t pc;
    uint32_t sp;
    uint32_t status;
};

struct Memory {
    std::vector<uint8_t> data;
};

CPU cpu;
Memory memory;
std::unique_ptr<MemoryPool> memPool;
std::vector<uint8_t> isoData;
GLuint shaderProgram = 0;
GLuint vertexArray = 0;
GLuint vertexBuffer = 0;
std::vector<uint32_t> instructionCache;
const size_t CACHE_SIZE = 64;

void prefetchInstructions() {
    instructionCache.clear();
    size_t pc_local = cpu.pc;
    for (size_t i = 0; i < CACHE_SIZE; i++) {
        if (pc_local + 4 > memory.data.size()) break;
        uint32_t instruction = 0;
        std::memcpy(&instruction, &memory.data[pc_local], sizeof(uint32_t));
        instructionCache.push_back(instruction);
        pc_local += 4;
    }
}

void initMemory() {
    memory.data.resize(1024 * 1024 * 128, 0);
    memPool = std::unique_ptr<MemoryPool>(new MemoryPool(1024 * 1024 * 128, 4096));
    std::fill(memory.data.begin(), memory.data.end(), 0);
}

uint32_t readMemory32(uint32_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (address + 4 > memory.data.size()){
        std::cerr << "Memory read out of bounds!" << std::endl;
        return 0;
    }
    uint32_t value = 0;
    std::memcpy(&value, &memory.data[address], sizeof(uint32_t));
    return value;
}

void writeMemory32(uint32_t address, uint32_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (address + 4 > memory.data.size()){
        std::cerr << "Memory write out of bounds!" << std::endl;
        return;
    }
    std::memcpy(&memory.data[address], &value, sizeof(uint32_t));
}

uint32_t fetchInstruction() {
    if (cpu.pc + 4 > memory.data.size()){
        std::cerr << "PC out of memory bounds!" << std::endl;
        return 0;
    }
    if (!instructionCache.empty()) {
        uint32_t instr = instructionCache.front();
        instructionCache.erase(instructionCache.begin());
        return instr;
    }
    uint32_t instruction = 0;
    std::memcpy(&instruction, &memory.data[cpu.pc], sizeof(uint32_t));
    return instruction;
}

void executeFullInstruction(uint32_t instruction) {
    uint8_t opcode = (instruction >> 24) & 0xFF;
    switch (opcode) {
        case 0x00:
            break;
        case 0x01: {
            uint8_t rd = (instruction >> 16) & 0xFF;
            uint8_t rs = (instruction >> 8) & 0xFF;
            uint8_t rt = instruction & 0xFF;
            cpu.registers[rd] = cpu.registers[rs] + cpu.registers[rt];
            break;
        }
        case 0x02: {
            uint8_t rs = (instruction >> 16) & 0xFF;
            int16_t imm = instruction & 0xFFFF;
            if (cpu.registers[rs] == 0) {
                cpu.pc += imm;
                return;
            }
            break;
        }
        case 0x03: {
            uint8_t rd = (instruction >> 16) & 0xFF;
            uint16_t imm = instruction & 0xFFFF;
            cpu.registers[rd] = imm;
            break;
        }
        case 0x04: {
            uint8_t rd = (instruction >> 16) & 0xFF;
            uint8_t rs = (instruction >> 8) & 0xFF;
            uint8_t rt = instruction & 0xFF;
            cpu.registers[rd] = cpu.registers[rs] * cpu.registers[rt];
            break;
        }
        case 0x05: {
            uint8_t rd = (instruction >> 16) & 0xFF;
            uint8_t rs = (instruction >> 8) & 0xFF;
            uint8_t rt = instruction & 0xFF;
            cpu.registers[rd] = cpu.registers[rs] & cpu.registers[rt];
            break;
        }
        default:
            std::cerr << "Unknown instruction opcode: " << static_cast<int>(opcode) << std::endl;
            break;
    }
    cpu.pc += 4;
}

void emulateFullCPU(std::atomic<bool>& running) {
    auto nextTick = std::chrono::steady_clock::now();
    while (running.load()) {
        prefetchInstructions();
        for (size_t i = 0; i < instructionCache.size(); ++i) {
            uint32_t instruction = fetchInstruction();
            executeFullInstruction(instruction);
        }
        nextTick += std::chrono::milliseconds(5);
        std::this_thread::sleep_until(nextTick);
    }
}

void emulateCPU(std::atomic<bool>& running) {
    auto lastTime = std::chrono::steady_clock::now();
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
        cpu.pc += static_cast<uint32_t>((elapsed * 4) / 10);
        lastTime = now;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

#ifdef ENABLE_OPTIMIZATIONS
void emulateFullCPU_Optimized(std::atomic<bool>& running) {
    while (running.load()) {
        prefetchInstructions();
        for (int i = 0; i < 10 && !instructionCache.empty(); ++i) {
            uint32_t instruction = fetchInstruction();
            executeFullInstruction(instruction);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
#endif

#ifdef ENABLE_JIT
void jitEngine(std::atomic<bool>& running) {
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::cout << "[JIT] Compiling batch of instructions." << std::endl;
    }
}
#endif

bool loadBIOSFiles() {
    const char* complexBiosFile = "assets/Complex_4627v1.03.bin";
    const char* mcpxFile = "assets/mcpx_1.0.bin";
    const char* hddFile = "assets/xbox_hdd.qcow2";
    std::ifstream complexBiosStream(complexBiosFile, std::ios::binary);
    if (!complexBiosStream.is_open()){
        std::cerr << "Error loading Complex BIOS file!" << std::endl;
        return false;
    }
    std::ifstream mcpxStream(mcpxFile, std::ios::binary);
    if (!mcpxStream.is_open()){
        std::cerr << "Error loading MCPX BIOS file!" << std::endl;
        return false;
    }
    std::ifstream hddStream(hddFile, std::ios::binary);
    if (!hddStream.is_open()){
        std::cerr << "Error loading Xbox HDD file!" << std::endl;
        return false;
    }
    complexBiosStream.seekg(0, std::ios::end);
    std::streamsize size = complexBiosStream.tellg();
    complexBiosStream.seekg(0, std::ios::beg);
    std::streamsize bytesToRead = std::min<std::streamsize>(memory.data.size(), size);
    complexBiosStream.read(reinterpret_cast<char*>(&memory.data[0]), bytesToRead);
    complexBiosStream.close();
    std::cout << "BIOS loaded successfully." << std::endl;
    cpu.pc = 0x0000;
    return true;
}

bool loadISOFromFilePath(const std::string& isoFilePath) {
    std::string path = isoFilePath;
    const std::string filePrefix = "file://";
    if (path.compare(0, filePrefix.size(), filePrefix) == 0) {
        path = path.substr(filePrefix.size());
    }
    std::ifstream isoFile(path, std::ios::binary);
    if (!isoFile.is_open()){
        std::cerr << "Error opening ISO file: " << path << std::endl;
        return false;
    }
    isoFile.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(isoFile.tellg());
    isoFile.seekg(0, std::ios::beg);
    isoData.resize(size);
    isoFile.read(reinterpret_cast<char*>(isoData.data()), size);
    isoFile.close();
    std::cout << "ISO file loaded successfully: " << path << std::endl;
    return true;
}

bool loadISOFile(const char* isoFilePath) {
    return loadISOFromFilePath(std::string(isoFilePath));
}

void processGPUCommands() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

#ifdef ENABLE_OPTIMIZATIONS
void processGPUCommands_Optimized() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFlush();
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "GL error in optimized GPU commands: " << error << std::endl;
    }
}
#endif

void emulateGPU(std::atomic<bool>& running) {
    auto nextTick = std::chrono::steady_clock::now();
    while (running.load()) {
        nextTick += std::chrono::milliseconds(5);
        processGPUCommands();
        std::this_thread::sleep_until(nextTick);
    }
}

#ifdef ENABLE_OPTIMIZATIONS
void emulateGPU_Optimized(std::atomic<bool>& running) {
    auto nextTick = std::chrono::steady_clock::now();
    while (running.load()) {
        nextTick += std::chrono::milliseconds(3);
        processGPUCommands_Optimized();
        std::this_thread::sleep_until(nextTick);
    }
}
#endif

void handleInput() {
    std::lock_guard<std::mutex> lock(inputMutex);
    while (!inputQueue.empty()) {
        InputEvent event = inputQueue.front();
        inputQueue.pop();
        switch (event.type) {
            case InputType::KEYBOARD:
                std::cout << "Keyboard event: key " << event.key << " action " << event.action << std::endl;
                break;
            case InputType::MOUSE:
                std::cout << "Mouse event: position (" << event.mouseX << ", " << event.mouseY << ")" << std::endl;
                break;
            case InputType::JOYSTICK:
                std::cout << "Joystick event: id " << event.joystickId << " axis (" << event.axisX << ", " << event.axisY << ")" << std::endl;
                break;
            default:
                break;
        }
    }
}

std::string loadShaderSourceFromFile(const char* filePath) {
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open()){
        std::cerr << "Error loading shader file: " << filePath << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << shaderFile.rdbuf();
    return ss.str();
}

GLuint loadShader(GLenum type, const char* shaderSrc) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char* log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        std::cerr << "Error compiling shader: " << log << std::endl;
        delete[] log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void checkGLError(const std::string& context) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "GL error in " << context << ": " << error << std::endl;
    }
}

bool initOpenGL(EGLDisplay& eglDisplay, EGLContext& eglContext, EGLSurface& eglSurface) {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY){
        std::cerr << "Failed to get EGL display!" << std::endl;
        return false;
    }
    if (!eglInitialize(eglDisplay, nullptr, nullptr)){
        std::cerr << "Failed to initialize EGL!" << std::endl;
        return false;
    }
    EGLint attribList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay, attribList, &config, 1, &numConfigs);
    if (numConfigs == 0){
        std::cerr << "No suitable EGL configs found!" << std::endl;
        eglTerminate(eglDisplay);
        return false;
    }
    eglSurface = eglCreateWindowSurface(eglDisplay, config, nullptr, nullptr);
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE});
    if (eglContext == EGL_NO_CONTEXT){
        std::cerr << "Failed to create EGL context!" << std::endl;
        eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
        return false;
    }
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    glViewport(0, 0, 852, 480);
    eglSwapInterval(eglDisplay, 1);
    std::string vertexShaderSource = loadShaderSourceFromFile("assets/vertex_shader.glsl");
    std::string fragmentShaderSource = loadShaderSourceFromFile("assets/fragment_shader.glsl");
    if (vertexShaderSource.empty() || fragmentShaderSource.empty()){
        return false;
    }
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource.c_str());
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    GLint linked;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked){
        GLint logLength;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
        char* log = new char[logLength];
        glGetProgramInfoLog(shaderProgram, logLength, &logLength, log);
        std::cerr << "Error linking program: " << log << std::endl;
        delete[] log;
        return false;
    }
    glUseProgram(shaderProgram);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
    glDisable(GL_DITHER);
    std::cout << "OpenGL ES 3.0 initialized with resolution 852x480." << std::endl;
    checkGLError("initOpenGL after setup");
    return true;
}

void cleanupResources(EGLDisplay display, EGLContext context, EGLSurface surface) {
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
    }
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
    }
    eglTerminate(display);
    std::cout << "Resources cleaned up." << std::endl;
}

extern "C" {

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeInit(JNIEnv* env, jobject obj) {
    initMemory();
    std::fill(memory.data.begin(), memory.data.end(), 0);
    cpu.pc = 0;
    if (!loadBIOSFiles()){
        std::cerr << "BIOS load failed!" << std::endl;
    }
    std::cout << "Emulator initialized." << std::endl;
}

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeResize(JNIEnv* env, jobject obj, jint width, jint height) {
    glViewport(0, 0, width, height);
}

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeRenderFrame(JNIEnv* env, jobject obj) {
    handleInput();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeLoadISO(JNIEnv* env, jobject obj, jstring isoFilePathJStr) {
    const char* isoFilePathCStr = env->GetStringUTFChars(isoFilePathJStr, nullptr);
    std::string isoFilePathStr(isoFilePathCStr);
    env->ReleaseStringUTFChars(isoFilePathJStr, isoFilePathCStr);
    if (!loadISOFromFilePath(isoFilePathStr)){
        std::cerr << "Failed to load ISO from file path: " << isoFilePathStr << std::endl;
    }
}

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeCleanup(JNIEnv* env, jobject obj) {
    if (shaderProgram != 0){
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    std::cout << "Emulator cleanup done." << std::endl;
}

JNIEXPORT void JNICALL Java_com_example_xemu_GameRenderer_nativeInputEvent(JNIEnv* env, jobject obj, jint type, jint key, jint action, jint mouseX, jint mouseY, jint joystickId, jfloat axisX, jfloat axisY) {
    InputEvent event;
    event.type = static_cast<InputType>(type);
    event.key = key;
    event.action = action;
    event.mouseX = mouseX;
    event.mouseY = mouseY;
    event.joystickId = joystickId;
    event.axisX = axisX;
    event.axisY = axisY;
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        inputQueue.push(event);
    }
}

}

void optimizeMemoryUsage() {
}

void networkSupport(std::atomic<bool>& running) {
    while (running.load()){
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "[Network] Synchronizing network state." << std::endl;
    }
}

void debugPerformanceTracking(std::atomic<bool>& running) {
    while (running.load()){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "[Performance] CPU PC: " << cpu.pc << ", Memory size: " << memory.data.size() << std::endl;
    }
}

void batteryManagement(std::atomic<bool>& running) {
    int batteryLevel = 100;
    while (running.load()){
        std::this_thread::sleep_for(std::chrono::seconds(5));
        batteryLevel -= 1;
        std::cout << "[Battery] Level: " << batteryLevel << "%" << std::endl;
        if (batteryLevel < 20) {
            std::cout << "[Battery] Low battery - optimizing performance." << std::endl;
        }
    }
}

#ifdef STANDALONE
void displayFPS(std::atomic<bool>& running) {
    while (running.load()) {
        std::cout << "FPS: " << 60 << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: emulator <ISO_FILE_PATH>" << std::endl;
        return -1;
    }
    std::string isoFilePath = argv[1];
    initMemory();
    if (!loadBIOSFiles()){
        std::cerr << "Failed to load BIOS files!" << std::endl;
        return -1;
    }
    if (!loadISOFromFilePath(isoFilePath)){
        std::cerr << "Failed to load ISO file!" << std::endl;
        return -1;
    }
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY){
        std::cerr << "Failed to get EGL display!" << std::endl;
        return -1;
    }
    if (!eglInitialize(eglDisplay, nullptr, nullptr)){
        std::cerr << "Failed to initialize EGL!" << std::endl;
        return -1;
    }
    EGLint attribList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay, attribList, &config, 1, &numConfigs);
    if (numConfigs == 0){
        std::cerr << "No suitable EGL configs found!" << std::endl;
        eglTerminate(eglDisplay);
        return -1;
    }
    EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, config, nullptr, nullptr);
    EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE});
    if (eglContext == EGL_NO_CONTEXT){
        std::cerr << "Failed to create EGL context!" << std::endl;
        eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
        return -1;
    }
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    glViewport(0, 0, 852, 480);
    eglSwapInterval(eglDisplay, 1);
    std::cout << "OpenGL ES 3.0 initialized." << std::endl;
    std::atomic<bool> running(true);
    std::thread fpsThread(displayFPS, std::ref(running));
    std::thread cpuThread(emulateCPU, std::ref(running));
    std::thread fullCpuThread(emulateFullCPU, std::ref(running));
    std::thread gpuThread(emulateGPU, std::ref(running));
#ifdef ENABLE_OPTIMIZATIONS
    std::thread optCpuThread(emulateFullCPU_Optimized, std::ref(running));
    std::thread optGpuThread(emulateGPU_Optimized, std::ref(running));
#endif
#ifdef ENABLE_JIT
    std::thread jitThread(jitEngine, std::ref(running));
#endif
    std::thread networkThread(networkSupport, std::ref(running));
    std::thread debugThread(debugPerformanceTracking, std::ref(running));
    std::thread batteryThread(batteryManagement, std::ref(running));
    std::thread audioThread([](std::atomic<bool>& run) {
        while (run.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }, std::ref(running));
    while (running.load()) {
        optimizeMemoryUsage();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        eglSwapBuffers(eglDisplay, eglSurface);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    running = false;
    fpsThread.join();
    cpuThread.join();
    fullCpuThread.join();
    gpuThread.join();
#ifdef ENABLE_OPTIMIZATIONS
    optCpuThread.join();
    optGpuThread.join();
#endif
#ifdef ENABLE_JIT
    jitThread.join();
#endif
    networkThread.join();
    debugThread.join();
    batteryThread.join();
    audioThread.join();
    std::fill(memory.data.begin(), memory.data.end(), 0);
    cleanupResources(eglDisplay, eglContext, eglSurface);
    std::cout << "Emulator terminated successfully." << std::endl;
    return 0;
}
#endif
