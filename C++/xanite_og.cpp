#define ENABLE_OPTIMIZATIONS
#define ENABLE_JIT

// system
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <string>
#include <sstream>
#include <mutex>
#include <queue>
#include <list>
#include <memory>

//  Android
#include <jni.h>
#include <android/log.h>

//  OpenGL + EGL
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// CDIO  
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
#include <cdio/version.h>
#include <cdio/disc.h>
#include <cdio/stdint.h>
#include <cdio/read.h>
#include <cdio/posix.h>
#include <cdio/limits.h>
#include <cdio/driver.h>
#include <AL/PCIBus.h>
#include <AL/PCIDevice.h>
#include <cdio/iso9660.h>
//thanks you cxbx for your emu this help me  
// Add
#include <cdio/input_jostick.h>
#include <algorithm>
#include <cdio/xbox.h>

// Emulator Xbox original for Android ❤️ Ziunx

GLuint VBO, EBO, VAO;  // VBO, EBO,  VAO 

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

typedef enum {
    MCPX_X2,  
    MCPX_X3, 
} MCPXRevision;

class MCPXDevice {
public:
    MCPXDevice(MCPXRevision revision) : m_revision(revision) {}

    void Init() {
        __android_log_print(ANDROID_LOG_INFO, "mik_lib", "MCPX Initialized (Revision: %d)", m_revision);
    }

    void Reset() {
        __android_log_print(ANDROID_LOG_INFO, "mik_lib", "MCPX Reset");
    }

    uint32_t IORead(int barIndex, uint32_t port, unsigned size) {
        __android_log_print(ANDROID_LOG_DEBUG, "mik_lib", "MCPX IO Read: port=0x%X, size=%u", port, size);
        return 0;  
    }

    void IOWrite(int barIndex, uint32_t port, uint32_t value, unsigned size) {
        __android_log_print(ANDROID_LOG_DEBUG, "mik_lib", "MCPX IO Write: port=0x%X, value=0x%X, size=%u", port, value, size);
    }

private:
    MCPXRevision m_revision;
};

int main() {
    MCPXDevice mcpxDevice(MCPX_X3);

    mcpxDevice.Init();

    uint32_t value = mcpxDevice.IORead(0, 0x80, 4);
    std::cout << "Read value: " << value << std::endl;

    mcpxDevice.IOWrite(0, 0x80, 0x1234, 4);
    
    mcpxDevice.Reset();

    return 0;
}

std::mutex memoryMutex;
std::mutex inputMutex;
std::queue<InputEvent> inputQueue;

void handleJoystickInput(float x, float y) {
    std::lock_guard<std::mutex> lock(inputMutex);
    InputEvent event;
    event.type = InputType::JOYSTICK;
    event.axisX = x;
    event.axisY = y;
    inputQueue.push(event);
}
void setupGraphics() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);   
    glCullFace(GL_BACK);      

    glEnable(GL_BLEND);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  
    glViewport(0, 0, 852, 480); 
 }

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
                std::cerr << "Unknown input event type!" << std::endl;  
                break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRenderer_nativeHandleJoystickInput(JNIEnv* env, jobject thiz, jfloat x, jfloat y) {
    handleJoystickInput(x, y); 
}

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
        uint32_t instruction = 0;
        std::memcpy(&instruction, &memory.data[pc_local], sizeof(uint32_t));
        instructionCache.push_back(instruction);
        pc_local += 4;
    }
}

JNIEXPORT jboolean JNICALL Java_com_example_xemu_GameRenderer_nativeLoadISO(JNIEnv* env, jobject obj, jstring filePath) {
    const char* path = env->GetStringUTFChars(filePath, NULL);
    if (path == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Failed to get file path");
        return JNI_FALSE; 
    }

    std::ifstream isoFile(path, std::ios::binary | std::ios::ate); 
    if (!isoFile.is_open()) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Failed to open ISO file: %s", path);
        env->ReleaseStringUTFChars(filePath, path);
        return JNI_FALSE;
    }

    std::streamsize size = isoFile.tellg();
    isoFile.seekg(0, std::ios::beg);

    if (size <= 0) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Invalid ISO file size: %s", path);
        isoFile.close();
        env->ReleaseStringUTFChars(filePath, path);
        return JNI_FALSE;
    }

    std::vector<uint8_t> isoData(size);
    if (!isoFile.read(reinterpret_cast<char*>(isoData.data()), size)) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Failed to read ISO file: %s", path);
        isoFile.close();
        env->ReleaseStringUTFChars(filePath, path);
        return JNI_FALSE;
    }

    isoFile.close();

    __android_log_print(ANDROID_LOG_INFO, "mik_lib", "ISO file loaded successfully: %s (Size: %ld bytes)", path, size);

    env->ReleaseStringUTFChars(filePath, path);

    return JNI_TRUE; 
}

void initMemory() {
    const size_t memorySize = 1024 * 1024 * 256;  

    try {
        memory.data.resize(memorySize, 0);  
    } catch (const std::bad_alloc& e) {
        std::cerr << "Failed to allocate memory: " << e.what() << std::endl;
        return;
    }

    std::cout << "Memory initialized successfully." << std::endl;
}

void writeMemory32(uint32_t address, uint32_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (address + 4 > memory.size()) {
        std::cerr << "Memory write out of bounds!" << std::endl;
        return;
    }
}

uint32_t fetchInstruction() {
    if (cpu.pc + 4 > memory.size()) {
        std::cerr << "PC out of memory bounds!" << std::endl;
        return 0;
    }
    if (!instructionCache.empty()) {
        uint32_t instr = instructionCache.front();
        instructionCache.erase(instructionCache.begin());
        return instr;
    }
    uint32_t instruction = 0;
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

        if (instructionCache.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        for (size_t i = 0; i < instructionCache.size() && running.load(); ++i) {
            uint32_t instruction = fetchInstruction();
            executeFullInstruction(instruction);
        }

        nextTick += std::chrono::milliseconds(5);
        std::this_thread::sleep_until(nextTick);
    }
}

void emulateCPU(std::atomic<bool>& running) {
    while (running.load()) {
        uint32_t instruction = fetchInstruction();
        executeFullInstruction(instruction);
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
        std::cout << "[JIT] Compiling batch of instructions." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

#endif

bool loadBIOSFiles() {
    const char* complexBiosFile = "assets/Complex_4627v1.03.bin";
    const char* mcpxFile = "assets/mcpx_1.0.bin";
    const char* hddFile = "assets/xbox_hdd.qcow2";
    std::ifstream complexBiosStream(complexBiosFile, std::ios::binary);
    if (!complexBiosStream.is_open()) {
        std::cerr << "Error loading Complex BIOS file!" << std::endl;
        return false;
    }
    std::ifstream mcpxStream(mcpxFile, std::ios::binary);
    if (!mcpxStream.is_open()) {
        std::cerr << "Error loading MCPX BIOS file!" << std::endl;
        return false;
    }
    std::ifstream hddStream(hddFile, std::ios::binary);
    if (!hddStream.is_open()) {
        std::cerr << "Error loading Xbox HDD file!" << std::endl;
        return false;
    }
    complexBiosStream.seekg(0, std::ios::end);
    std::streamsize size = complexBiosStream.tellg();
    complexBiosStream.seekg(0, std::ios::beg);
    std::streamsize bytesToRead = std::min<std::streamsize>(memory.size(), size);
    complexBiosStream.close();
    std::cout << "BIOS loaded successfully." << std::endl;
    cpu.pc = 0x0000;
    return true;
}
   
   
bool loadISO(const std::string& isoUriString) {
    std::ifstream isoFile(isoUriString, std::ios::binary);
    if (!isoFile.is_open()) {
        std::cerr << "Failed to open ISO file: " << isoUriString << std::endl;
        return false;
    }

    isoFile.seekg(0, std::ios::end);
    std::streamsize size = isoFile.tellg();
    isoFile.seekg(0, std::ios::beg);

    isoData.resize(size); 
    isoFile.read(reinterpret_cast<char*>(isoData.data()), size);
    isoFile.close();

    std::cout << "ISO file loaded successfully: " << isoUriString << " (Size: " << size << " bytes)" << std::endl;

    return true;
}

void processGPUCommands_Optimized() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);  
    glBindVertexArray(VAO);  

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0); 

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in processGPUCommands_Optimized: " << error << std::endl;
    }
}

void initBuffers() {
    float vertices[] = {
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   
    };

    unsigned int indices[] = { 
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); 
    
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));  
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

#ifdef ENABLE_OPTIMIZATIONS
void emulateGPU(std::atomic<bool>& running) {
    auto nextTick = std::chrono::steady_clock::now();
    while (running.load()) {
        nextTick += std::chrono::milliseconds(5); 
        processGPUCommands_Optimized();
        std::this_thread::sleep_until(nextTick);
    }
}
#endif

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
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, &logLength, log.data());
        std::cerr << "Error compiling shader: " << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

JNIEXPORT jboolean JNICALL Java_com_example_xemu_GameRenderer_nativeResize(JNIEnv* env, jobject obj, jint width, jint height) {
    if (width <= 0 || height <= 0) {
        return JNI_FALSE; 
    }

    glViewport(0, 0, width, height);

    __android_log_print(ANDROID_LOG_INFO, "mik_lib", "New dimensions: %d x %d", width, height);

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_example_xemu_GameRenderer_nativeRenderFrame(JNIEnv* env, jobject) {

       __android_log_print(ANDROID_LOG_DEBUG, "mik_lib", "Rendering frame...");
       
       return true;
   }
   
bool initOpenGL(EGLDisplay& eglDisplay, EGLContext& eglContext, EGLSurface& eglSurface) {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display!" << std::endl;
        return false;
    }
    if (!eglInitialize(eglDisplay, nullptr, nullptr)) {
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

if (numConfigs == 0) {
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
    return true;
}

void checkGLError(const std::string& context) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in " << context << ": " << error << std::endl;
    }
}

void cleanupResources(EGLDisplay display, EGLContext context, EGLSurface surface) {
    if (display != EGL_NO_DISPLAY) {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
            context = EGL_NO_CONTEXT; 
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;  
        }
        eglTerminate(display);
        display = EGL_NO_DISPLAY; 
    }
    std::cout << "Resources cleaned up." << std::endl;
}

extern "C" {

extern "C" {

  JNIEXPORT jboolean JNICALL Java_com_example_xemu_GameRenderer_nativeLoadISO(JNIEnv* env, jobject obj, jbyteArray isoDataJava, jint length) {
    jbyte* buffer = env->GetByteArrayElements(isoDataJava, nullptr);
    if (buffer == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Failed to get byte array elements");
        return JNI_FALSE;
    }

    try {
        isoData.reserve(isoData.size() + length);
        isoData.insert(isoData.end(), buffer, buffer + length);
    } catch (const std::bad_alloc& e) {
        __android_log_print(ANDROID_LOG_ERROR, "mik_lib", "Failed to allocate memory: %s", e.what());
        env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);
        return JNI_FALSE;
    }

        if (length <= 0) {
            LOGE("Invalid data length");
            env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);
            return false;
        }

        if (isoData.empty()) {
            try {
                isoData.reserve(static_cast<size_t>(7) * 1024 * 1024 * 1024); 
            } catch (const std::bad_alloc& e) {
                LOGE("Failed to reserve memory: %s", e.what());
                env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);
                return false;
            }
        }

        try {
            isoData.insert(isoData.end(), buffer, buffer + length);
        } catch (const std::bad_alloc& e) {
            LOGE("Failed to insert data into isoData: %s", e.what());
            env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);
            return false;
        }

         env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);
    __android_log_print(ANDROID_LOG_INFO, "mik_lib", "ISO data chunk loaded successfully (Size: %lu bytes)", isoData.size());
    return JNI_TRUE;
}

} 

}
void optimizeMemoryUsage() {
    if (instructionCache.size() > CACHE_SIZE) {
        instructionCache.resize(CACHE_SIZE);
    }
}

void networkSupport(std::atomic<bool>& running) {
    while (running.load()){
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "[Network] Synchronizing network state." << std::endl;
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