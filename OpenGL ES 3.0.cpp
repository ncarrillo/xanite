#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window_jni.h>

#define LOG_TAG "XEMU"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

EGLDisplay display;
EGLSurface surface;
EGLContext context;
EGLConfig config;

bool initOpenGL(ANativeWindow* window) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return false;
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("Failed to initialize EGL");
        return false;
    }

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("Failed to choose EGL config");
        return false;
    }

    surface = eglCreateWindowSurface(display, config, window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL window surface");
        return false;
    }

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("Failed to make EGL context current");
        return false;
    }

    LOGI("OpenGL initialized successfully");
    return true;
}

void renderFrame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // شاشة سوداء
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    eglSwapBuffers(display, surface);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRunActivity_startGame(JNIEnv* env, jobject thiz, jobject surface, jstring isoPath, jstring biosPath, jstring mcpxPath, jstring hddPath) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get native window");
        return;
    }

    if (!initOpenGL(window)) {
        LOGE("OpenGL initialization failed");
        ANativeWindow_release(window);
        return;
    }

    // بدء رسم الإطارات (render loop)
    bool running = true;
    while (running) {
        renderFrame();  // رسم الإطار
    }

    ANativeWindow_release(window);
}