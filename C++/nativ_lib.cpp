#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#define LOG_TAG "nativ_lib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const uint8_t XBOX_SIGNATURE[4] = {0x58, 0x42, 0x4F, 0x58};

bool findXboxSignature(const uint8_t* data, size_t size) {
    if (size < 4) {
        return false;
    }

    for (size_t i = 0; i <= size - 4; i++) {
        if (memcmp(data + i, XBOX_SIGNATURE, 4) == 0) {
            return true;
        }
    }

    return false;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_xemu_GamePlayViewModel_nativLoadGame(JNIEnv* env, jobject thiz, jobject context, jobject uri) {
    jclass uriClass = env->GetObjectClass(uri);
    jmethodID toStringMethod = env->GetMethodID(uriClass, "toString", "()Ljava/lang/String;");
    jstring uriString = (jstring) env->CallObjectMethod(uri, toStringMethod);
    const char* uriCStr = env->GetStringUTFChars(uriString, nullptr);

    LOGI("Loading game from URI: %s", uriCStr);

    jclass contextClass = env->GetObjectClass(context);
    jmethodID getContentResolverMethod = env->GetMethodID(contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(context, getContentResolverMethod);

    jclass contentResolverClass = env->GetObjectClass(contentResolver);
    jmethodID openInputStreamMethod = env->GetMethodID(contentResolverClass, "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;");
    jobject inputStream = env->CallObjectMethod(contentResolver, openInputStreamMethod, uri);

    if (inputStream == nullptr) {
        LOGE("Failed to open input stream");
        env->ReleaseStringUTFChars(uriString, uriCStr);
        return false;
    }

    jclass inputStreamClass = env->GetObjectClass(inputStream);
    jmethodID readMethod = env->GetMethodID(inputStreamClass, "read", "([B)I");

    const int bufferSize = 1 * 1024 * 1024;
    jbyteArray byteArray = env->NewByteArray(bufferSize);

    if (byteArray == nullptr) {
        LOGE("Failed to create byte array");
        env->CallVoidMethod(inputStream, env->GetMethodID(inputStreamClass, "close", "()V"));
        env->ReleaseStringUTFChars(uriString, uriCStr);
        return false;
    }

    jbyte* buffer = (jbyte*) malloc(bufferSize);

    if (buffer == nullptr) {
        LOGE("Failed to allocate memory for buffer");
        env->DeleteLocalRef(byteArray);
        env->CallVoidMethod(inputStream, env->GetMethodID(inputStreamClass, "close", "()V"));
        env->ReleaseStringUTFChars(uriString, uriCStr);
        return false;
    }

    jint bytesRead = env->CallIntMethod(inputStream, readMethod, byteArray);
    if (bytesRead > 0) {
        env->GetByteArrayRegion(byteArray, 0, bytesRead, buffer);

        if (!findXboxSignature((uint8_t*)buffer, bytesRead)) {
            LOGE("File is not a valid Xbox Original ISO");
            free(buffer);
            env->DeleteLocalRef(byteArray);
            env->CallVoidMethod(inputStream, env->GetMethodID(inputStreamClass, "close", "()V"));
            env->ReleaseStringUTFChars(uriString, uriCStr);
            return false;
        }
    }

    long totalBytesRead = bytesRead;

    const int chunkSize = 5 * 1024 * 1024;
    jbyte* chunkBuffer = (jbyte*) malloc(chunkSize);

    if (chunkBuffer == nullptr) {
        LOGE("Failed to allocate memory for chunk buffer");
        free(buffer);
        env->DeleteLocalRef(byteArray);
        env->CallVoidMethod(inputStream, env->GetMethodID(inputStreamClass, "close", "()V"));
        env->ReleaseStringUTFChars(uriString, uriCStr);
        return false;
    }

    while (true) {
        bytesRead = env->CallIntMethod(inputStream, readMethod, byteArray);
        if (bytesRead == -1) {
            break;
        }

        env->GetByteArrayRegion(byteArray, 0, bytesRead, chunkBuffer);

        totalBytesRead += bytesRead;
        LOGI("Total bytes read: %ld", totalBytesRead);
    }

    free(buffer);
    free(chunkBuffer);
    env->DeleteLocalRef(byteArray);

    jmethodID closeMethod = env->GetMethodID(inputStreamClass, "close", "()V");
    env->CallVoidMethod(inputStream, closeMethod);
    env->ReleaseStringUTFChars(uriString, uriCStr);

    LOGI("Xbox Original ISO loaded successfully. Total bytes: %ld", totalBytesRead);
    return true;
}

void* ReserveMemory(size_t size) {
    void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        LOGE("Failed to reserve memory");
        return nullptr;
    }
    return memory;
}

void FreeMemory(void* memory, size_t size) {
    munmap(memory, size);
}

void* LoadEmulationLibrary(const char* libraryName) {
    void* library = dlopen(libraryName, RTLD_LAZY);
    if (!library) {
        LOGE("Failed to load library: %s", dlerror());
    }
    return library;
}

void* GetLibraryFunction(void* library, const char* functionName) {
    void* function = dlsym(library, functionName);
    if (!function) {
        LOGE("Failed to find function: %s", dlerror());
    }
    return function;
}