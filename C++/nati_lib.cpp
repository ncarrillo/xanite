#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>

#define TAG "nati_lib"


GLuint VBO, EBO, VAO, shaderProgram, texture;
std::vector<uint8_t> isoData;
std::atomic<bool> isGameLoaded{false};
std::mutex renderMutex;


void checkGLError(const char* context) {
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        const char* errorName;
        switch (error) {
            case GL_INVALID_ENUM: errorName = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorName = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorName = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorName = "GL_OUT_OF_MEMORY"; break;
            default: errorName = "UNKNOWN_ERROR"; break;
        }
        __android_log_print(ANDROID_LOG_ERROR, TAG, "OpenGL error in %s: %s", context, errorName);
    }
}

GLuint loadShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Shader compilation failed: %s", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint loadTexture(const unsigned char* data, int width, int height) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRenderer_nativeInit(JNIEnv* env, jobject thiz) {
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Native library initialized");


    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
    glEnable(GL_DEPTH_TEST);

    const char* vertexShaderSource = R"(
        #version 300 es
      #version 300 es
layout(location = 0) in vec3 aPos;     
layout(location = 1) in vec3 aNormal;    
layout(location = 2) in vec2 aTexCoord;  

out vec3 FragPos;       
out vec3 Normal;      
out vec2 TexCoord;     

uniform mat4 model;  
uniform mat4 view;     
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0)); 
    Normal = mat3(transpose(inverse(model))) * aNormal; 
    TexCoord = aTexCoord;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
 )";

    const char* fragmentShaderSource = R"(
       #version 300 es
precision highp float;

in vec3 FragPos;        
in vec3 Normal;         
in vec2 TexCoord;      

out vec4 FragColor;     

uniform sampler2D ourTexture; 
uniform vec3 lightPos;       
uniform vec3 viewPos;         
uniform vec3 lightColor;    

void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * texture(ourTexture, TexCoord).rgb;
    FragColor = vec4(result, 1.0);
}
 )";

      GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    if (vertexShader == 0 || fragmentShader == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to load shaders");
        return JNI_FALSE;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Shader program linking failed: %s", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return JNI_FALSE;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(shaderProgram);
    
    float vertices[] = {
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 
         0.0f,  0.5f, 0.0f,  0.5f, 1.0f  
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRenderer_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height) {
    if (width <= 0 || height <= 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Invalid dimensions: %d x %d", width, height);
        return JNI_FALSE;
    }

    glViewport(0, 0, 852, 480);
    __android_log_print(ANDROID_LOG_INFO, TAG, "Resized to: %d x %d", width, height);

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRenderer_nativeRenderFrame(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(renderMutex);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    if (texture != 0) {
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    glDrawArrays(GL_TRIANGLES, 0, 3);

    checkGLError("glDrawArrays");

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRenderer_nativeLoadISO(JNIEnv* env, jobject thiz, jbyteArray isoDataJava, jint length) {
    jbyte* buffer = env->GetByteArrayElements(isoDataJava, nullptr);
    if (buffer == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get byte array elements");
        return JNI_FALSE;
    }

    isoData.insert(isoData.end(), buffer, buffer + length);

    env->ReleaseByteArrayElements(isoDataJava, buffer, JNI_ABORT);

    __android_log_print(ANDROID_LOG_INFO, TAG, "ISO data chunk loaded successfully (Size: %lu bytes)", isoData.size());

    unsigned char textureData[] = {
        255, 0, 0,   0, 255, 0,   0, 0, 255,
        0, 255, 0,   255, 0, 0,   0, 0, 255,
        0, 0, 255,   255, 0, 0,   0, 255, 0
    };
    texture = loadTexture(textureData, 3, 3);

    isGameLoaded = true;
    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GameRenderer_nativeCleanup(JNIEnv* env, jobject thiz) {
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Native resources cleaned up");

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture);

    return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_xemu_GameRenderer_nativeHandleJoystickInput(JNIEnv* env, jobject thiz, jfloat x, jfloat y) {
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Joystick input: x=%f, y=%f", x, y);
}