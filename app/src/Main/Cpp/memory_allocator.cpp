#include "memory_allocator.h"
#include <fstream>
#include <android/log.h>

#define LOG_TAG "MemoryAllocator"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)


MemoryAllocator::MemoryAllocator(uint32_t baseAddress, uint32_t size)
    : baseAddress(baseAddress), totalSize(size) {
}

MemoryAllocator::~MemoryAllocator() {
}

uint32_t MemoryAllocator::allocate(uint32_t size, uint32_t alignment, const std::string& purpose) {
    uint32_t address = baseAddress; 
    blocks.push_back({address, size, true, purpose});
    return address;
}

bool MemoryAllocator::deallocate(uint32_t address) {
    for (auto& block : blocks) {
        if (block.address == address && block.allocated) {
            block.allocated = false;
            return true;
        }
    }
    return false;
}

bool readFileFully(const std::string& path, std::vector<uint8_t>& buffer, size_t expectedSize) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open file: %s", path.c_str());
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < expectedSize) {
        LOGE("File size (%zu) less than expected (%zu)", fileSize, expectedSize);
        return false;
    }

    buffer.resize(expectedSize);

    size_t totalRead = 0;
    while (totalRead < expectedSize) {
        file.read(reinterpret_cast<char*>(buffer.data() + totalRead), expectedSize - totalRead);
        size_t justRead = file.gcount();
        if (justRead == 0) {
            LOGE("Unexpected EOF while reading file: %s", path.c_str());
            return false;
        }
        totalRead += justRead;
    }

    LOGI("Successfully read %zu bytes from %s", totalRead, path.c_str());
    return true;
}

int main() {
    const std::string filePath = "/data/user/0/com.xanite/files/XboxDashboard/xboxdash.xbe";
    const size_t expectedSize = 1961984;

    std::vector<uint8_t> buffer;

    if (readFileFully(filePath, buffer, expectedSize)) {
        LOGI("File read successfully with size: %zu bytes", buffer.size());
    } else {
        LOGE("Failed to read the file properly.");
    }

    return 0;
}