#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <memory>
#include <algorithm>
#include <android/log.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

class XboxMemory {
public:
    
    static constexpr uint32_t RAM_BASE = 0x00000000;
    static constexpr uint32_t RAM_SIZE = 128 * 1024 * 1024;
    
    static constexpr uint32_t BIOS_BASE = 0xFF000000;
    static constexpr uint32_t BIOS_SIZE = 1 * 1024 * 1024; 
    
    static constexpr uint32_t GPU_BASE = 0xFD000000;
    static constexpr uint32_t GPU_SIZE = 0x01000000; 
    
    static constexpr uint32_t APU_BASE = 0xFE000000;
    static constexpr uint32_t APU_SIZE = 0x01000000; 
    
    static constexpr uint32_t PCI_BASE = 0x80000000;
    static constexpr uint32_t PCI_SIZE = 0x01000000; 

    static constexpr uint32_t CACHE_BLOCK_SIZE = 64 * 1024; 
    static constexpr uint32_t CACHE_BLOCK_COUNT = 8;

    XboxMemory();
    ~XboxMemory();

    uint8_t read8(uint32_t address);
    uint16_t read16(uint32_t address);
    uint32_t read32(uint32_t address);
    uint64_t read64(uint32_t address);
#ifdef __ARM_NEON
    uint32x4_t read128(uint32_t address);
#endif

    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    void write32(uint32_t address, uint32_t value);
    void write64(uint32_t address, uint64_t value);
#ifdef __ARM_NEON
    void write128(uint32_t address, uint32x4_t value);
#endif

    bool loadBios(const std::string& path);
    void reset();

    void dmaTransfer(uint32_t src, uint32_t dest, uint32_t size);
    void dmaTransferNEON(uint32_t src, uint32_t dest, uint32_t size);

    bool mapRegion(uint32_t base, uint32_t size,
                 std::function<uint32_t(uint32_t)> readHandler,
                 std::function<void(uint32_t, uint32_t)> writeHandler);
    
    void unmapRegion(uint32_t base);
   
    void setAccessCallback(std::function<void(uint32_t, uint32_t, bool, uint32_t)> callback);

    uint8_t* getRamPointer();
    const uint8_t* getBiosPointer() const;
    uint32_t getRamSize() const;
    uint32_t getBiosSize() const;
    
    void flushCaches();
    void flushCacheRange(uint32_t address, uint32_t size);
    void invalidateCacheRange(uint32_t address, uint32_t size);

private:
 
    std::vector<uint8_t> ram;
    std::vector<uint8_t> bios;
    
    mutable std::mutex memoryMutex;

    struct MappedRegion {
        uint32_t base;
        uint32_t size;
        std::function<uint32_t(uint32_t)> readHandler;
        std::function<void(uint32_t, uint32_t)> writeHandler;
        
        bool contains(uint32_t addr) const {
            return addr >= base && addr < (base + size);
        }
    };
    std::vector<MappedRegion> mappedRegions;
    
    std::function<void(uint32_t, uint32_t, bool, uint32_t)> accessCallback;
   
    struct MemoryCache {
        uint32_t base;
        uint32_t size;
        std::unique_ptr<uint8_t[]> data;
        bool dirty;
        
        MemoryCache() : base(0), size(0), data(nullptr), dirty(false) {}
        
        bool contains(uint32_t addr) const {
            return addr >= base && addr < base + size;
        }
    };
    std::vector<MemoryCache> memoryCaches;

    MappedRegion* findMappedRegion(uint32_t address);
    void updateCache(uint32_t address, uint32_t size);
    void allocateCacheBlock(MemoryCache& cache);
    
    uint32_t handleGPURead(uint32_t address);
    void handleGPUWrite(uint32_t address, uint32_t value);
    uint32_t handleAPURead(uint32_t address);
    void handleAPUWrite(uint32_t address, uint32_t value);
};