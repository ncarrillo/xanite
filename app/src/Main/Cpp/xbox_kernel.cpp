#include "xbox_kernel.h"
#include "xbox_utils.h"
#include "memory_allocator.h"
#include <android/log.h>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <map>
#include <list>
#include <string>
#include <functional>
#include <vector>
#include <cstring>
#include <cstdio>

#ifndef LOG_TAG
#define LOG_TAG "XboxKernel"
#endif

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

constexpr uint32_t XBOX_PAGE_SIZE = 0x1000;
constexpr uint32_t MAX_THREADS = 256;
constexpr uint32_t SECTION_FLAG_LOADED = 0x00000001;

XboxKernel::XboxKernel(XboxMemory* memory, X86Core* cpu) : 
    memory(memory),
    cpu(cpu),
    currentThreadId(0),
    currentProcessId(0),
    nextThreadId(1),
    xbeLoaded(false),
    memoryAllocator(std::make_unique<MemoryAllocator>(0x10000000, 0x10000000))
{
   
    memset(&currentXbe, 0, sizeof(XbeHeader));
    sections.clear();
    xbeData.clear();
    threads.clear();
    
    initializeSyscalls();
    initializeMemory();
    initializeThreads();
}

XboxKernel::~XboxKernel() {
   
    for (auto& block : memoryBlocks) {
        if (block.allocated) {
            freeMemory(block.address);
        }
    }
}

void XboxKernel::initializeSyscalls() {
    
    registerSyscall(0x0001, [this](uint32_t* args) { return syscallDebugPrint(args); });
    registerSyscall(0x0002, [this](uint32_t* args) { return syscallAllocateMemory(args); });
    registerSyscall(0x0003, [this](uint32_t* args) { return syscallFreeMemory(args); });
    registerSyscall(0x0004, [this](uint32_t* args) { return syscallCreateThread(args); });
    registerSyscall(0x0005, [this](uint32_t* args) { return syscallOpenFile(args); });
    registerSyscall(0x0006, [this](uint32_t* args) { return syscallReadFile(args); });
    registerSyscall(0x0007, [this](uint32_t* args) { return syscallWriteFile(args); });
    registerSyscall(0x0008, [this](uint32_t* args) { return syscallCloseFile(args); });

    syscallNames[0x0001] = "DebugPrint";
    syscallNames[0x0002] = "AllocateMemory";
    syscallNames[0x0003] = "FreeMemory";
    syscallNames[0x0004] = "CreateThread";
    syscallNames[0x0005] = "OpenFile";
    syscallNames[0x0006] = "ReadFile";
    syscallNames[0x0007] = "WriteFile";
    syscallNames[0x0008] = "CloseFile";
}

void XboxKernel::initializeMemory() {
    
    MemoryBlock reservedBlock;
    reservedBlock.address = 0x00010000;
    reservedBlock.size = 0x10000;
    reservedBlock.allocated = false;
    reservedBlock.purpose = "Reserved";
    memoryBlocks.push_back(reservedBlock);
   
    MemoryBlock mainMemoryBlock;
    mainMemoryBlock.address = 0x10000000;
    mainMemoryBlock.size = 0x10000000;
    mainMemoryBlock.allocated = false;
    mainMemoryBlock.purpose = "Main Memory";
    memoryBlocks.push_back(mainMemoryBlock);
    
    MemoryBlock highMemoryBlock;
    highMemoryBlock.address = 0xE0000000;
    highMemoryBlock.size = 0x4000000;
    highMemoryBlock.allocated = false;
    highMemoryBlock.purpose = "High Memory";
    memoryBlocks.push_back(highMemoryBlock);
}

void XboxKernel::initializeThreads() {
    
    Thread mainThread;
    mainThread.id = nextThreadId++;
    mainThread.processId = 0;
    mainThread.entryPoint = 0;
    mainThread.stackPointer = 0;
    mainThread.status = ThreadStatus::Running;
    mainThread.priority = ThreadPriority::Normal;
    
    threads.push_back(mainThread);
    currentThreadId = mainThread.id;
}

bool XboxKernel::loadXbe(const std::string& path) {
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open XBE file: %s", path.c_str());
        return false;
    }
    
    const std::streamsize FIXED_FILE_SIZE = 1961984; 
    std::streamsize fileSize = file.tellg();
    
    if (fileSize != FIXED_FILE_SIZE) {
        LOGW("File size mismatch: expected %zu, got %zu. Forcing fixed size.", 
             FIXED_FILE_SIZE, fileSize);
        fileSize = FIXED_FILE_SIZE;
    }

    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char*>(&currentXbe), sizeof(XbeHeader));
    if (file.gcount() != sizeof(XbeHeader)) {
        LOGE("Failed to read XBE header: expected %zu, got %zd", sizeof(XbeHeader), file.gcount());
        return false;
    }

    if (memcmp(currentXbe.magic, "XBEH", 4) != 0) {
        LOGE("Invalid XBE magic");
        return false;
    }

    currentXbe.sizeOfImage = FIXED_FILE_SIZE;

    xbeData.resize(FIXED_FILE_SIZE);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(xbeData.data()), FIXED_FILE_SIZE);
    
    if (file.gcount() != FIXED_FILE_SIZE) {
        LOGW("Partial read of XBE file: expected %zu, got %zd. Padding with zeros.", 
             FIXED_FILE_SIZE, file.gcount());
        
        std::fill(xbeData.begin() + file.gcount(), xbeData.end(), 0);
    }

    loadSections();
    
    LOGI("XBE Loaded with fixed size %zu KB", FIXED_FILE_SIZE / 1024);

    xbeLoaded = true;
    return true;
}

void XboxKernel::unloadXbe() {
    if (!xbeLoaded) return;
    
    for (auto& section : sections) {
        if (section.flags & SECTION_FLAG_LOADED) {
            freeMemory(section.virtualAddr);
        }
    }
    
    xbeData.clear();
    sections.clear();
    xbeLoaded = false;
    memset(&currentXbe, 0, sizeof(XbeHeader));
}

bool XboxKernel::isXbeLoaded() const {
    return xbeLoaded;
}

uint32_t XboxKernel::getEntryPoint() const {
    return currentXbe.baseAddr + currentXbe.entryPoint;
}

void XboxKernel::loadSections() {
    if (!xbeLoaded) return;

    sections.clear();
    sections.resize(currentXbe.numberOfSections);

    uint8_t* sectionTable = xbeData.data() + 
                           (currentXbe.sectionHeadersAddr - currentXbe.baseAddr);
    
    for (uint32_t i = 0; i < currentXbe.numberOfSections; i++) {
        XbeSection* section = reinterpret_cast<XbeSection*>(sectionTable + i * sizeof(XbeSection));
        sections[i] = *section;

        
        if (section->flags & SECTION_FLAG_LOADED) {
            
            uint32_t allocSize = alignToPage(section->virtualSize);
            uint32_t address = allocateMemory(allocSize, XBOX_PAGE_SIZE, "XBE Section");
            
            if (address == 0) {
                LOGE("Failed to allocate memory for section %d at 0x%08X", i, section->virtualAddr);
                continue;
            }
               
            section->virtualAddr = address;
                   
            uint8_t* dest = memory->getRamPointer() + address;
            
            uint8_t* src = xbeData.data() + section->fileAddr;
            size_t copySize = std::min(section->virtualSize, section->fileSize);
            memcpy(dest, src, copySize);
            
            if (section->virtualSize > copySize) {
                memset(dest + copySize, 0, section->virtualSize - copySize);
            }
            
            LOGI("Loaded section %d: VA=0x%08X, Size=%d bytes, Flags=0x%08X",
                 i, address, section->virtualSize, section->flags);
        }
    }
}

uint32_t XboxKernel::handleSyscall(uint32_t call, uint32_t* args) {
  
    auto nameIt = syscallNames.find(call);
    const char* callName = (nameIt != syscallNames.end()) ? nameIt->second.c_str() : "Unknown";
    
    LOGI("Syscall %s (0x%04X) Args: 0x%08X, 0x%08X, 0x%08X, 0x%08X", 
         callName, call, args[0], args[1], args[2], args[3]);

    auto handler = syscallTable.find(call);
    if (handler != syscallTable.end()) {
        return handler->second(args);
    }

    LOGE("Unknown syscall: 0x%04X", call);
    return syscallUnknown(args);
}

void XboxKernel::registerSyscall(uint32_t callId, std::function<uint32_t(uint32_t*)> handler) {
    syscallTable[callId] = handler;
}

void XboxKernel::setDebugOutput(std::function<void(const std::string&)> callback) {
    debugOutput = callback;
}

uint32_t XboxKernel::allocateMemory(uint32_t size, uint32_t alignment, const char* purpose) {
    
    if (alignment == 0) alignment = XBOX_PAGE_SIZE;
    uint32_t addr = memoryAllocator->allocate(size, alignment, purpose);
    
    if (addr != 0) {
        MemoryBlock block;
        block.address = addr;
        block.size = size;
        block.allocated = true;
        block.purpose = purpose;
        memoryBlocks.push_back(block);
        return addr;
    }

    size = alignToPage(size);
    alignment = std::max(alignment, XBOX_PAGE_SIZE);

    for (auto it = memoryBlocks.begin(); it != memoryBlocks.end(); ++it) {
        if (!it->allocated && it->size >= size) {
            uint32_t alignedAddr = (it->address + alignment - 1) & ~(alignment - 1);
            uint32_t endAddr = alignedAddr + size;
            uint32_t blockEnd = it->address + it->size;

            if (endAddr <= blockEnd) {
                
                if (alignedAddr > it->address) {
                    MemoryBlock before;
                    before.address = it->address;
                    before.size = alignedAddr - it->address;
                    before.allocated = false;
                    before.purpose = "Free";
                    memoryBlocks.insert(it, before);
                }

                if (endAddr < blockEnd) {
                    MemoryBlock after;
                    after.address = endAddr;
                    after.size = blockEnd - endAddr;
                    after.allocated = false;
                    after.purpose = "Free";
                    memoryBlocks.insert(std::next(it), after);
                }

                it->address = alignedAddr;
                it->size = size;
                it->allocated = true;
                it->purpose = purpose;

                return alignedAddr;
            }
        }
    }

    LOGE("Memory allocation failed: size=0x%X, align=0x%X, purpose=%s", size, alignment, purpose);
    return 0;
}

uint32_t XboxKernel::allocateMemory(uint32_t size) {
    return allocateMemory(size, XBOX_PAGE_SIZE, "User Allocation");
}

bool XboxKernel::freeMemory(uint32_t address) {
    
    if (address >= 0x10000000 && address < 0x20000000) {
        memoryAllocator->deallocate(address);
       
        for (auto it = memoryBlocks.begin(); it != memoryBlocks.end(); ++it) {
            if (it->address == address && it->allocated) {
                memoryBlocks.erase(it);
                return true;
            }
        }
        return true;
    }

    for (auto it = memoryBlocks.begin(); it != memoryBlocks.end(); ++it) {
        if (it->address == address && it->allocated) {
            it->allocated = false;
            it->purpose = "Freed";

            if (it != memoryBlocks.begin()) {
                auto prev = std::prev(it);
                if (!prev->allocated && prev->address + prev->size == address) {
                    prev->size += it->size;
                    memoryBlocks.erase(it);
                    it = prev;
                }
            }

            auto next = std::next(it);
            if (next != memoryBlocks.end() && !next->allocated && 
                it->address + it->size == next->address) {
                it->size += next->size;
                memoryBlocks.erase(next);
            }

            return true;
        }
    }

    LOGE("Memory free failed: address 0x%08X not found", address);
    return false;
}

uint32_t XboxKernel::createThread(uint32_t entryPoint, uint32_t stackSize) {
    if (threads.size() >= MAX_THREADS) {
        LOGE("Thread limit reached (%d)", MAX_THREADS);
        return 0;
    }

    uint32_t stackAddr = allocateMemory(stackSize, XBOX_PAGE_SIZE, "Thread Stack");
    if (!stackAddr) {
        LOGE("Failed to allocate thread stack");
        return 0;
    }

    Thread thread;
    thread.id = nextThreadId++;
    thread.processId = currentProcessId;
    thread.entryPoint = entryPoint;
    thread.stackPointer = stackAddr + stackSize - 4; 
    thread.status = ThreadStatus::Ready;
    thread.priority = ThreadPriority::Normal;

    threads.push_back(thread);
    return thread.id;
}

uint32_t XboxKernel::alignToPage(uint32_t size) {
    return (size + (XBOX_PAGE_SIZE - 1)) & ~(XBOX_PAGE_SIZE - 1);
}

uint32_t XboxKernel::syscallUnknown(uint32_t* args) {
    LOGE("Unknown syscall: args=[0x%08X, 0x%08X, 0x%08X, 0x%08X]",
         args[0], args[1], args[2], args[3]);
    return 0xFFFFFFFF;
}

uint32_t XboxKernel::syscallDebugPrint(uint32_t* args) {
    const char* message = reinterpret_cast<const char*>(args[0]);
    if (debugOutput) debugOutput(message);
    LOGI("Xbox Debug: %s", message);
    return 0;
}

uint32_t XboxKernel::syscallAllocateMemory(uint32_t* args) {
    uint32_t size = args[0];
    uint32_t alignment = args[1] ? args[1] : XBOX_PAGE_SIZE;
    return allocateMemory(size, alignment, "Syscall Allocation");
}

uint32_t XboxKernel::syscallFreeMemory(uint32_t* args) {
    return freeMemory(args[0]) ? 0 : 0xFFFFFFFF;
}

uint32_t XboxKernel::syscallCreateThread(uint32_t* args) {
    uint32_t entry = args[0];
    uint32_t stack = args[1];
    uint32_t priority = args[2] & 0xF;

    if (threads.size() >= MAX_THREADS) {
        LOGE("Thread limit reached");
        return 0;
    }

    Thread thread;
    thread.id = nextThreadId++;
    thread.processId = currentProcessId;
    thread.entryPoint = entry;
    thread.stackPointer = stack;
    thread.status = ThreadStatus::Ready;
    thread.priority = static_cast<ThreadPriority>(priority);

    threads.push_back(thread);
    return thread.id;
}

uint32_t XboxKernel::syscallOpenFile(uint32_t* args) {
    const char* path = reinterpret_cast<const char*>(args[0]);
    int mode = args[1];
    
    int fd = open(path, mode);
    return (fd >= 0) ? fd : 0xFFFFFFFF;
}

uint32_t XboxKernel::syscallReadFile(uint32_t* args) {
    int fd = args[0];
    void* buffer = reinterpret_cast<void*>(args[1]);
    size_t size = args[2];
    
    ssize_t result = read(fd, buffer, size);
    return (result >= 0) ? result : 0xFFFFFFFF;
}

uint32_t XboxKernel::syscallWriteFile(uint32_t* args) {
    int fd = args[0];
    const void* buffer = reinterpret_cast<const void*>(args[1]);
    size_t size = args[2];
    
    ssize_t result = write(fd, buffer, size);
    return (result >= 0) ? result : 0xFFFFFFFF;
}

uint32_t XboxKernel::syscallCloseFile(uint32_t* args) {
    int fd = args[0];
    return (close(fd) == 0) ? 0 : 0xFFFFFFFF;
}

uint32_t XboxKernel::createProcess(const char* name, uint32_t entryPoint) {
    uint32_t pid = ++currentProcessId;
    uint32_t tid = createThread(entryPoint, 0x10000);
    if (!tid) return 0;
    
    LOGI("Created process %s (PID: %d) with main thread %d", name, pid, tid);
    return pid;
}

bool XboxKernel::terminateThread(uint32_t threadId) {
    for (auto& thread : threads) {
        if (thread.id == threadId) {
            
            if (thread.stackPointer) {
                freeMemory(thread.stackPointer - 0x10000 + 4);
            }
            
            thread.status = ThreadStatus::Terminated;
            
            if (threadId == currentThreadId) {
                schedule();
            }
            return true;
        }
    }
    return false;
}

void XboxKernel::schedule() {
  
    for (auto& thread : threads) {
        if (thread.status == ThreadStatus::Ready) {
            
            for (auto& t : threads) {
                if (t.id == currentThreadId) {
                    t.status = ThreadStatus::Ready;
                    break;
                }
            }
            
            thread.status = ThreadStatus::Running;
            currentThreadId = thread.id;
            break;
        }
    }
}

void XboxKernel::dumpMemoryMap() const {
    if (!debugOutput) return;
    
    std::string output = "Memory Map:\n";
    for (const auto& block : memoryBlocks) {
        char line[128];
        snprintf(line, sizeof(line), "0x%08X-0x%08X (%6u KB) [%s] %s\n",
                block.address,
                block.address + block.size - 1,
                block.size / 1024,
                block.allocated ? "ALLOC" : "FREE ",
                block.purpose.c_str());
        output += line;
    }
    debugOutput(output);
}

void XboxKernel::dumpThreads() const {
    if (!debugOutput) return;
    
    std::string output = "Thread List:\n";
    for (const auto& thread : threads) {
        const char* status = "";
        switch (thread.status) {
            case ThreadStatus::Ready: status = "Ready"; break;
            case ThreadStatus::Running: status = "Running"; break;
            case ThreadStatus::Terminated: status = "Terminated"; break;
        }
        
        char line[128];
        snprintf(line, sizeof(line), "%4d: EP=0x%08X, SP=0x%08X, %s, Prio=%d\n",
                thread.id,
                thread.entryPoint,
                thread.stackPointer,
                status,
                static_cast<int>(thread.priority));
        output += line;
    }
    debugOutput(output);
}

bool XboxKernel::validateXbe() {
    return xbeLoaded && 
           memcmp(currentXbe.magic, "XBEH", 4) == 0 &&
           currentXbe.baseAddr >= 0x10000;
}