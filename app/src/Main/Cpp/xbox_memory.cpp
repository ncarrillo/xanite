#include "xbox_memory.h"
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>

#define LOG_TAG "XboxMemory"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

XboxMemory::XboxMemory() : 
    ram(RAM_SIZE, 0),
    bios(BIOS_SIZE, 0),
    memoryCaches(CACHE_BLOCK_COUNT) {
    
    for (auto& cache : memoryCaches) {
        allocateCacheBlock(cache);
    }
    
    mapRegion(GPU_BASE, GPU_SIZE,
        [this](uint32_t addr) { return handleGPURead(addr); },
        [this](uint32_t addr, uint32_t val) { handleGPUWrite(addr, val); });
    
    mapRegion(APU_BASE, APU_SIZE,
        [this](uint32_t addr) { return handleAPURead(addr); },
        [this](uint32_t addr, uint32_t val) { handleAPUWrite(addr, val); });
}

XboxMemory::~XboxMemory() {
  
}

void XboxMemory::allocateCacheBlock(MemoryCache& cache) {
    cache.data = std::make_unique<uint8_t[]>(CACHE_BLOCK_SIZE);
    cache.base = 0;
    cache.size = 0;
    cache.dirty = false;
}

uint8_t XboxMemory::read8(uint32_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address)) {
            return cache.data.get()[address - cache.base];
        }
    }

    if (address >= RAM_BASE && address < RAM_BASE + RAM_SIZE) {
        return ram[address - RAM_BASE];
    }
    
    if (address >= BIOS_BASE && address < BIOS_BASE + BIOS_SIZE) {
        return bios[address - BIOS_BASE];
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32_t value = region->readHandler(address);
        if (accessCallback) accessCallback(address, value, false, 1);
        return static_cast<uint8_t>(value);
    }
    
    LOGE("Read8 from unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

uint16_t XboxMemory::read16(uint32_t address) {
    if (address % 2 != 0) {
        LOGE("Unaligned read16 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 1)) {
            return *reinterpret_cast<uint16_t*>(&cache.data[address - cache.base]);
        }
    }

    if (address >= RAM_BASE && address + 1 < RAM_BASE + RAM_SIZE) {
        return *reinterpret_cast<uint16_t*>(&ram[address - RAM_BASE]);
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32_t value = region->readHandler(address);
        if (accessCallback) accessCallback(address, value, false, 2);
        return static_cast<uint16_t>(value);
    }
    
    LOGE("Read16 from unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

uint32_t XboxMemory::read32(uint32_t address) {
    if (address % 4 != 0) {
        LOGE("Unaligned read32 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 3)) {
            return *reinterpret_cast<uint32_t*>(&cache.data[address - cache.base]);
        }
    }

    if (address >= RAM_BASE && address + 3 < RAM_BASE + RAM_SIZE) {
        uint32_t value;
memcpy(&value, &ram[address - RAM_BASE], sizeof(value));
return value;
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32_t value = region->readHandler(address);
        if (accessCallback) accessCallback(address, value, false, 4);
        return value;
    }
    
    LOGE("Read32 from unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

uint64_t XboxMemory::read64(uint32_t address) {
    if (address % 8 != 0) {
        LOGE("Unaligned read64 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 7)) {
            return *reinterpret_cast<uint64_t*>(&cache.data[address - cache.base]);
        }
    }

    if (address >= RAM_BASE && address + 7 < RAM_BASE + RAM_SIZE) {
        return *reinterpret_cast<uint64_t*>(&ram[address - RAM_BASE]);
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint64_t value = region->readHandler(address);
        value |= static_cast<uint64_t>(region->readHandler(address + 4)) << 32;
        if (accessCallback) accessCallback(address, static_cast<uint32_t>(value), false, 8);
        return value;
    }
    
    LOGE("Read64 from unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

uint32x4_t XboxMemory::read128(uint32_t address) {
    if (address % 16 != 0) {
        LOGE("Unaligned read128 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (address >= RAM_BASE && address + 15 < RAM_BASE + RAM_SIZE) {
        return vld1q_u32(reinterpret_cast<uint32_t*>(&ram[address - RAM_BASE]));
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32x4_t result;
        for (int i = 0; i < 4; i++) {
            result[i] = region->readHandler(address + i*4);
        }
        if (accessCallback) accessCallback(address, 0, false, 16);
        return result;
    }
    
    LOGE("Read128 from unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

void XboxMemory::write8(uint32_t address, uint8_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address)) {
            cache.data[address - cache.base] = value;
            cache.dirty = true;
            if (accessCallback) accessCallback(address, value, true, 1);
            return;
        }
    }

    if (address >= RAM_BASE && address < RAM_BASE + RAM_SIZE) {
        ram[address - RAM_BASE] = value;
        if (accessCallback) accessCallback(address, value, true, 1);
        return;
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32_t current = region->readHandler(address);
        current = (current & 0xFFFFFF00) | value;
        region->writeHandler(address, current);
        if (accessCallback) accessCallback(address, value, true, 1);
        return;
    }
    
    LOGE("Write8 to unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

void XboxMemory::write16(uint32_t address, uint16_t value) {
    if (address % 2 != 0) {
        LOGE("Unaligned write16 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 1)) {
            *reinterpret_cast<uint16_t*>(&cache.data[address - cache.base]) = value;
            cache.dirty = true;
            if (accessCallback) accessCallback(address, value, true, 2);
            return;
        }
    }

    if (address >= RAM_BASE && address + 1 < RAM_BASE + RAM_SIZE) {
        *reinterpret_cast<uint16_t*>(&ram[address - RAM_BASE]) = value;
        if (accessCallback) accessCallback(address, value, true, 2);
        return;
    }
    
    if (auto* region = findMappedRegion(address)) {
        uint32_t current = region->readHandler(address);
        current = (current & 0xFFFF0000) | value;
        region->writeHandler(address, current);
        if (accessCallback) accessCallback(address, value, true, 2);
        return;
    }
    
    LOGE("Write16 to unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

void XboxMemory::write32(uint32_t address, uint32_t value) {
    if (address % 4 != 0) {
        LOGE("Unaligned write32 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 3)) {
            *reinterpret_cast<uint32_t*>(&cache.data[address - cache.base]) = value;
            cache.dirty = true;
            if (accessCallback) accessCallback(address, value, true, 4);
            return;
        }
    }

    if (address >= RAM_BASE && address + 3 < RAM_BASE + RAM_SIZE) {
        *reinterpret_cast<uint32_t*>(&ram[address - RAM_BASE]) = value;
        if (accessCallback) accessCallback(address, value, true, 4);
        return;
    }
    
    if (auto* region = findMappedRegion(address)) {
        region->writeHandler(address, value);
        if (accessCallback) accessCallback(address, value, true, 4);
        return;
    }
    
    LOGE("Write32 to unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

void XboxMemory::write64(uint32_t address, uint64_t value) {
    if (address % 8 != 0) {
        LOGE("Unaligned write64 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (auto& cache : memoryCaches) {
        if (cache.contains(address) && cache.contains(address + 7)) {
            *reinterpret_cast<uint64_t*>(&cache.data[address - cache.base]) = value;
            cache.dirty = true;
            if (accessCallback) accessCallback(address, static_cast<uint32_t>(value), true, 8);
            return;
        }
    }

    if (address >= RAM_BASE && address + 7 < RAM_BASE + RAM_SIZE) {
        *reinterpret_cast<uint64_t*>(&ram[address - RAM_BASE]) = value;
        if (accessCallback) accessCallback(address, static_cast<uint32_t>(value), true, 8);
        return;
    }
    
    if (auto* region = findMappedRegion(address)) {
        region->writeHandler(address, static_cast<uint32_t>(value));
        region->writeHandler(address + 4, static_cast<uint32_t>(value >> 32));
        if (accessCallback) accessCallback(address, static_cast<uint32_t>(value), true, 8);
        return;
    }
    
    LOGE("Write64 to unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

void XboxMemory::write128(uint32_t address, uint32x4_t value) {
    if (address % 16 != 0) {
        LOGE("Unaligned write128 at 0x%08X", address);
        throw std::runtime_error("Unaligned memory access");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (address >= RAM_BASE && address + 15 < RAM_BASE + RAM_SIZE) {
        vst1q_u32(reinterpret_cast<uint32_t*>(&ram[address - RAM_BASE]), value);
        if (accessCallback) accessCallback(address, 0, true, 16);
        return;
    }
    
    if (auto* region = findMappedRegion(address)) {
        for (int i = 0; i < 4; i++) {
            region->writeHandler(address + i*4, value[i]);
        }
        if (accessCallback) accessCallback(address, 0, true, 16);
        return;
    }
    
    LOGE("Write128 to unmapped address 0x%08X", address);
    throw std::runtime_error("Memory access violation");
}

bool XboxMemory::loadBios(const std::string& path) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        LOGE("Failed to open BIOS file: %s", path.c_str());
        return false;
    }
    
    bios.resize(BIOS_SIZE); 
    ssize_t bytesRead = read(fd, bios.data(), BIOS_SIZE);
    close(fd);
    
    if (bytesRead != BIOS_SIZE) {
        LOGE("Failed to read BIOS (expected %d bytes, got %d)", BIOS_SIZE, bytesRead);
        return false;
    }
    
    LOGI("BIOS (1MB) loaded successfully"); 
    return true;
}

void XboxMemory::reset() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    std::fill(ram.begin(), ram.end(), 0);
    
    for (auto& cache : memoryCaches) {
        cache.base = 0;
        cache.size = 0;
        cache.dirty = false;
    }
}

void XboxMemory::dmaTransfer(uint32_t src, uint32_t dest, uint32_t size) {
    if (size == 0) return;

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if ((src >= RAM_BASE && src + size <= RAM_BASE + RAM_SIZE) &&
        (dest >= RAM_BASE && dest + size <= RAM_BASE + RAM_SIZE)) {
        
        uint8_t* src_ptr = &ram[src - RAM_BASE];
        uint8_t* dest_ptr = &ram[dest - RAM_BASE];
        
        if (size >= 64 && (src % 16 == 0) && (dest % 16 == 0)) {
            uint32_t blocks = size / 16;
            uint32_t* src32 = reinterpret_cast<uint32_t*>(src_ptr);
            uint32_t* dest32 = reinterpret_cast<uint32_t*>(dest_ptr);
            
            for (uint32_t i = 0; i < blocks; i++) {
                uint32x4_t data = vld1q_u32(src32);
                vst1q_u32(dest32, data);
                src32 += 4;
                dest32 += 4;
            }
            
            uint32_t remaining = size % 16;
            if (remaining > 0) {
                memcpy(dest32, src32, remaining);
            }
        } else {
            memmove(dest_ptr, src_ptr, size);
        }
        
        updateCache(dest, size);
        return;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        uint8_t val = read8(src + i);
        write8(dest + i, val);
    }
}

void XboxMemory::dmaTransferNEON(uint32_t src, uint32_t dest, uint32_t size) {
    if (size == 0) return;

    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if ((src >= RAM_BASE && src + size <= RAM_BASE + RAM_SIZE) &&
        (dest >= RAM_BASE && dest + size <= RAM_BASE + RAM_SIZE)) {
        
        uint8_t* src_ptr = &ram[src - RAM_BASE];
        uint8_t* dest_ptr = &ram[dest - RAM_BASE];
        
        uint32_t blocks = size / 64;
        for (uint32_t i = 0; i < blocks; i++) {
            uint8x16x4_t data = vld4q_u8(src_ptr);
            vst4q_u8(dest_ptr, data);
            src_ptr += 64;
            dest_ptr += 64;
        }
        
        uint32_t remaining = size % 64;
        if (remaining) {
            memcpy(dest_ptr, src_ptr, remaining);
        }
        
        updateCache(dest, size);
    } else {
        dmaTransfer(src, dest, size);
    }
}

void XboxMemory::flushCaches() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    for (auto& cache : memoryCaches) {
        if (cache.dirty && cache.size > 0) {
            memcpy(&ram[cache.base - RAM_BASE], cache.data.get(), cache.size);
            cache.dirty = false;
        }
    }
}

void XboxMemory::flushCacheRange(uint32_t address, uint32_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    for (auto& cache : memoryCaches) {
        if (cache.dirty && cache.size > 0) {
            uint32_t start = std::max(address, cache.base);
            uint32_t end = std::min(address + size, cache.base + cache.size);
            if (start < end) {
                uint32_t len = end - start;
                memcpy(&ram[start - RAM_BASE], 
                      &cache.data[start - cache.base], 
                      len);
            }
        }
    }
}

void XboxMemory::invalidateCacheRange(uint32_t address, uint32_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    for (auto& cache : memoryCaches) {
        if (cache.size > 0) {
            uint32_t start = std::max(address, cache.base);
            uint32_t end = std::min(address + size, cache.base + cache.size);
            if (start < end) {
                uint32_t len = end - start;
                memcpy(&cache.data[start - cache.base], 
                      &ram[start - RAM_BASE], 
                      len);
            }
        }
    }
}

XboxMemory::MappedRegion* XboxMemory::findMappedRegion(uint32_t address) {
    for (auto& region : mappedRegions) {
        if (region.contains(address)) {
            return &region;
        }
    }
    return nullptr;
}

void XboxMemory::updateCache(uint32_t address, uint32_t size) {
    for (auto& cache : memoryCaches) {
        if (cache.size == 0) continue;
        
        uint32_t start = std::max(address, cache.base);
        uint32_t end = std::min(address + size, cache.base + cache.size);
        
        if (start < end) {
            uint32_t len = end - start;
            memcpy(&cache.data[start - cache.base], 
                   &ram[start - RAM_BASE], 
                   len);
        }
    }
}

uint32_t XboxMemory::handleGPURead(uint32_t address) {
    LOGI("GPU read at 0x%08X", address);
    return 0;
}

void XboxMemory::handleGPUWrite(uint32_t address, uint32_t value) {
    LOGI("GPU write at 0x%08X: 0x%08X", address, value);
}

uint32_t XboxMemory::handleAPURead(uint32_t address) {
    LOGI("APU read at 0x%08X", address);
    return 0;
}

void XboxMemory::handleAPUWrite(uint32_t address, uint32_t value) {
    LOGI("APU write at 0x%08X: 0x%08X", address, value);
}

bool XboxMemory::mapRegion(uint32_t base, uint32_t size,
                         std::function<uint32_t(uint32_t)> readHandler,
                         std::function<void(uint32_t, uint32_t)> writeHandler) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    for (const auto& region : mappedRegions) {
        if (region.base == base) {
            LOGE("Region 0x%08X already mapped", base);
            return false;
        }
    }
    
    mappedRegions.push_back({base, size, readHandler, writeHandler});
    LOGI("Mapped region 0x%08X-0x%08X", base, base + size);
    return true;
}

void XboxMemory::unmapRegion(uint32_t base) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = std::remove_if(mappedRegions.begin(), mappedRegions.end(),
        [base](const MappedRegion& region) { return region.base == base; });
    
    if (it != mappedRegions.end()) {
        mappedRegions.erase(it, mappedRegions.end());
        LOGI("Unmapped region 0x%08X", base);
    }
}

void XboxMemory::setAccessCallback(std::function<void(uint32_t, uint32_t, bool, uint32_t)> callback) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    accessCallback = callback;
}

uint8_t* XboxMemory::getRamPointer() {
    return ram.data();
}

const uint8_t* XboxMemory::getBiosPointer() const {
    return bios.data();
}

uint32_t XboxMemory::getRamSize() const {
    return RAM_SIZE;
}

uint32_t XboxMemory::getBiosSize() const {
    return BIOS_SIZE;
}