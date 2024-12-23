#include <cstdlib>
#include <fstream>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <chrono>
#include "Emulator.h"
#include <vector>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define LOG_TAG "XEMU"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

EGLDisplay display;
EGLSurface surface;
EGLContext context;
GLuint vertexArrayID, vertexBufferID;
GLuint shaderProgram;
GLuint frameCounter = 0, fps = 0;
std::chrono::time_point<std::chrono::high_resolution_clock> lastTime, lastFpsTime;
bool gameLoaded = false;

// تهيئة نظام الصوت باستخدام OpenSLES
SLObjectItf engineObject = nullptr;
SLEngineItf engineEngine = nullptr;
SLObjectItf outputMixObject = nullptr;

// تهيئة كائن Emulator
Emulator* emulator = nullptr;

// دالة لتفعيل الصوت باستخدام OpenSLES
void initAudio() {
    SLresult result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create OpenSLES engine");
        return;
    }

    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize OpenSLES engine");
        return;
    }

    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get OpenSLES engine interface");
        return;
    }

    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create OpenSLES output mix");
        return;
    }

    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize OpenSLES output mix");
        return;
    }

    LOGI("OpenSLES audio initialized");
}

// تحسين تهيئة OpenGL مع الشيدر
GLuint loadShader(GLenum type, const char* shaderSource) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("Error compiling shader: %s", infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void initShaders() {
    const char* vertexShaderSource = R"(
        #version 300 es
        layout(location = 0) in vec3 position;
        void main() {
            gl_Position = vec4(position, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 300 es
        precision mediump float;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(0.0, 1.0, 0.0, 1.0);  // لون أخضر كاختبار
        }
    )";

    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linked;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(shaderProgram, infoLen, nullptr, infoLog.data());
            LOGE("Error linking program: %s", infoLog.data());
        }
        glDeleteProgram(shaderProgram);
    }
    glUseProgram(shaderProgram);
}

// تحسين دالة تهيئة OpenGL وتخصيصها حسب احتياجات الألعاب الكبيرة
void initOpenGL(ANativeWindow* nativeWindow) {
    ANativeWindow_setBuffersGeometry(nativeWindow, 720, 540, 0);  // تعيين دقة أعلى للألعاب

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return;
    }

    EGLint numConfigs;
    EGLConfig config;

    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("Failed to initialize EGL");
        return;
    }

    eglChooseConfig(display, nullptr, &config, 1, &numConfigs);
    if (numConfigs <= 0) {
        LOGE("No EGL configurations found");
        return;
    }

    surface = eglCreateWindowSurface(display, config, nativeWindow, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL window surface");
        return;
    }

    EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("Failed to make EGL context current");
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    initShaders();
    initAudio();

    gameLoaded = true;
    lastTime = std::chrono::high_resolution_clock::now();
}

// دالة لتشغيل اللعبة
extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRunActivity_startNativeGame(JNIEnv *env, jobject thiz, jobject surface, jstring isoFilePath, jstring biosFilePath, jstring mcpxFilePath, jstring hddFilePath) {
    const char *isoPath = env->GetStringUTFChars(isoFilePath, nullptr);
    const char *biosPath = env->GetStringUTFChars(biosFilePath, nullptr);
    const char *mcpxPath = env->GetStringUTFChars(mcpxFilePath, nullptr);
    const char *hddPath = env->GetStringUTFChars(hddFilePath, nullptr);

    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);

    // فحص تهيئة OpenGL قبل محاولة تشغيل أي شيء
    if (!gameLoaded) {
        initOpenGL(nativeWindow);
        if (!gameLoaded) {
            LOGE("Failed to initialize OpenGL. Cannot start game.");
            return;  // الخروج إذا فشلت التهيئة
        }
    }

    // تهيئة Emulator إذا لم يكن مهيأً بالفعل
    if (emulator == nullptr) {
        emulator = new Emulator(nativeWindow, isoPath, biosPath, mcpxPath, hddPath);
        if (emulator != nullptr) {
            LOGI("Emulator initialized successfully with ISO, BIOS, MCPX, and HDD paths.");
        } else {
            LOGE("Failed to initialize emulator.");
        }
    } else {
        LOGI("Emulator is already initialized.");
    }

    env->ReleaseStringUTFChars(isoFilePath, isoPath);
    env->ReleaseStringUTFChars(biosFilePath, biosPath);
    env->ReleaseStringUTFChars(mcpxFilePath, mcpxPath);
    env->ReleaseStringUTFChars(hddFilePath, hddPath);
}