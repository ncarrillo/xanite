#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

#include <cstdint>
#include <string>
#include <vector>


class MemoryAllocator {
public:
    MemoryAllocator(uint32_t baseAddress, uint32_t size);
    ~MemoryAllocator();

    uint32_t allocate(uint32_t size, uint32_t alignment, const std::string& purpose);
    bool deallocate(uint32_t address);
    
private:
    struct MemoryBlock {
        uint32_t address;
        uint32_t size;
        bool allocated;
        std::string purpose;
    };

    std::vector<MemoryBlock> blocks;
    uint32_t baseAddress;
    uint32_t totalSize;
};

bool readFileFully(const std::string& path, std::vector<uint8_t>& buffer, size_t expectedSize);

#endif // MEMORY_ALLOCATOR_H