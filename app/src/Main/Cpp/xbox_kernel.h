#ifndef XBOX_KERNEL_H
#define XBOX_KERNEL_H

#include "memory_allocator.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstring>
#include "xbox_memory.h"

class XboxMemory;
class X86Core;

class XboxKernel {
public:
    XboxKernel(XboxMemory* memory, X86Core* cpu);
    ~XboxKernel();

    struct XbeHeader {
        char magic[4];
        uint32_t baseAddr;
        uint32_t sizeOfHeaders;
        uint32_t sizeOfImage;
        uint32_t sectionHeadersAddr;
        uint32_t numberOfSections;
        uint32_t timestamp;
        uint32_t certificateAddr;
        uint32_t entryPoint;
        uint32_t titleNameAddr;
        uint32_t numberOfLibraryImports;
        uint32_t libraryImportsAddr;
        uint32_t kernelLibraryImportAddr;
        uint32_t xapiLibraryImportAddr;
        uint32_t logoBitmapAddr;
        uint32_t logoBitmapSize;
    };

    struct XbeSection {
        uint32_t flags;
        uint32_t virtualAddr;
        uint32_t virtualSize;
        uint32_t fileAddr;
        uint32_t fileSize;
        std::string name;
    };

    enum class ThreadStatus {
        Ready,
        Running,
        Terminated
    };

    enum class ThreadPriority {
        Low,
        Normal,
        High
    };

    struct MemoryBlock {
        uint32_t address;
        uint32_t size;
        bool allocated;
        std::string purpose;
    };

    struct Thread {
        uint32_t id;
        uint32_t processId;
        uint32_t entryPoint;
        uint32_t stackPointer;
        ThreadStatus status;
        ThreadPriority priority;
    };

    bool loadXbe(const std::string& path);
    void unloadXbe();
    bool isXbeLoaded() const;
    bool parseXbe(const std::string& path);
    bool validateXbe();

    uint32_t handleSyscall(uint32_t call, uint32_t* args);
    void registerSyscall(uint32_t callId, std::function<uint32_t(uint32_t*)> handler);

    static uint32_t alignToPage(uint32_t size);

    uint32_t allocateMemory(uint32_t size);
    uint32_t allocateMemory(uint32_t size, uint32_t alignment, const char* purpose);
    bool freeMemory(uint32_t address);
    uint32_t findFreeMemory(uint32_t size);
    void dumpMemoryMap() const;

    uint32_t createThread(uint32_t entryPoint, uint32_t stackSize);
    bool terminateThread(uint32_t threadId);
    void schedule();
    void dumpThreads() const;

    uint32_t createProcess(const char* name, uint32_t entryPoint);
   
    void setDebugOutput(std::function<void(const std::string&)> callback);

    uint32_t getEntryPoint() const;
    void registerCustomLib(const std::string& name, std::function<uint32_t(uint32_t)> handler);

    int getLoadingProgress() const {
        if (totalResources == 0) return 100;
        return (loadedResources * 100) / totalResources;
    }

    void setTotalResources(int total) { totalResources = total; }
    void incrementLoadedResources() { loadedResources++; }
    void setLoadedResources(int loaded) { loadedResources = loaded; }

private:
    XboxMemory* memory;
    X86Core* cpu;
    
    XbeHeader currentXbe;
    std::vector<XbeSection> sections;
    std::vector<uint8_t> xbeData;
    bool xbeLoaded;

    std::unordered_map<uint32_t, std::function<uint32_t(uint32_t*)>> syscallTable;
    std::unordered_map<uint32_t, std::string> syscallNames;

    std::unordered_map<std::string, std::function<uint32_t(uint32_t)>> customLibs;

    std::vector<MemoryBlock> memoryBlocks;
    std::unique_ptr<MemoryAllocator> memoryAllocator; 

    std::vector<Thread> threads;
    uint32_t nextThreadId;
    uint32_t currentThreadId;
    uint32_t currentProcessId;

    std::function<void(const std::string&)> debugOutput;

    int loadedResources = 0;
    int totalResources = 1;

    void initializeSyscalls();
    void initializeMemory();
    void initializeThreads();
    void loadSections();

    uint32_t syscallUnknown(uint32_t* args);
    uint32_t syscallDebugPrint(uint32_t* args);
    uint32_t syscallAllocateMemory(uint32_t* args);
    uint32_t syscallFreeMemory(uint32_t* args);
    uint32_t syscallCreateThread(uint32_t* args);
    uint32_t syscallOpenFile(uint32_t* args);
    uint32_t syscallReadFile(uint32_t* args);
    uint32_t syscallWriteFile(uint32_t* args);
    uint32_t syscallCloseFile(uint32_t* args);
};

#endif // XBOX_KERNEL_H