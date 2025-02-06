#include <jni.h>
#include <string>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <cmath>

// تسجيل الدخول في Android
#define TAG "GameRunActivity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

static float cameraX = 0.0f, cameraY = 0.0f, cameraZ = -5.0f;  // إحداثيات الكاميرا
static float characterX = 0.0f, characterY = 0.0f, characterZ = 0.0f;  // إحداثيات الشخصية

// دالة لتحميل اللعبة من ملف ISO
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRunActivity_loadGameFromISO(JNIEnv *env, jobject obj, jstring isoPath) {
    const char *path = env->GetStringUTFChars(isoPath, NULL);
    
    LOGD("Loading game from ISO: %s", path);

    // هنا يمكنك إضافة كود لتحميل الـ ISO (محاكاة عملية التحميل هنا)
    env->ReleaseStringUTFChars(isoPath, path);
    return JNI_TRUE;
}

// دالة لتحريك الجويستيك (تحريك الشخصية أو الكاميرا)
extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRunActivity_handleJoystickMovement(JNIEnv *env, jobject obj, jfloat x, jfloat y) {
    LOGD("Joystick Movement - X: %f, Y: %f", x, y);

    // استخدام إحداثيات الجويستيك لتحريك الكاميرا أو الشخصية
    cameraX += x * 0.1f;  // تعديل سرعة الكاميرا بناءً على الجويستيك
    cameraY += y * 0.1f;

    // تحريك الشخصية
    characterX += x * 0.05f;
    characterY += y * 0.05f;
}

// دالة لتفعيل OpenGL وتحديث الإعدادات
extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRunActivity_initializeOpenGL(JNIEnv *env, jobject obj) {
    // تهيئة OpenGL: ضبط الخلفية وأبعاد العرض
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // لون الخلفية
    glEnable(GL_DEPTH_TEST);  // تمكين اختبار العمق لرؤية الأشياء عن قرب وعمق
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // مسح الشاشة
}

// دالة لرسم الشخصيات والكاميرا
extern "C" JNIEXPORT void JNICALL
Java_com_example_xemu_GameRunActivity_drawScene(JNIEnv *env, jobject obj) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // مسح الشاشة
    
    // هنا يمكن إضافة الكود لرسم الكائنات باستخدام OpenGL
    // لاحظ أنه تم استبدال glLoadIdentity بـ glLoadMatrixf مع مصفوفة هوية جديدة
    GLfloat identityMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glLoadMatrixf(identityMatrix); // إعادة تحميل مصفوفة التحويل
    
    // تحريك الكاميرا
    glTranslatef(cameraX, cameraY, cameraZ);
    
    // رسم الشخصية أو الكائنات هنا
    glPushMatrix();
    glTranslatef(characterX, characterY, characterZ);  // تحريك الشخصية
    // رسم الشكل (مثل مربع أو كرة هنا)
    glColor3f(1.0f, 0.0f, 0.0f); // تغيير اللون إلى الأحمر
    glBegin(GL_QUADS);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f( 1.0f, -1.0f, 0.0f);
    glVertex3f( 1.0f,  1.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f, 0.0f);
    glEnd();
    glPopMatrix();
    
    // عرض الكائنات
    glFlush();
}