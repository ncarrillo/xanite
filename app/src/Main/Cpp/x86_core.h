#pragma once
#include "xbox_memory.h"
#include "xbox_kernel.h"  
#include <array>
#include <functional>
#include <arm_neon.h>
#include <unordered_map>
#include <vector>
#include <string>

class XboxKernel;  
class X86Core {
public:
    explicit X86Core(XboxMemory* memory);
    ~X86Core();
    
    bool traceEnabled;
    
    void reset();
    void executeStep();
    void execute(uint32_t cycles);
    
    void handleInterrupt(uint8_t interrupt);
    void raiseException(uint8_t exception);
    
    uint32_t getRegister(uint8_t reg) const;
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getEIP() const;
    uint32_t getEFLAGS() const;
    void setEIP(uint32_t new_eip);

    enum class CpuState {
        Running,
        Halted,
        Error,
        DebugBreak
    };
    
    CpuState getState() const;
    void setState(CpuState newState);
    void setPC(uint32_t pc);
    void enableTracing(bool enabled);

    void setBreakpoint(uint32_t address, std::function<void()> callback);
    void clearBreakpoint(uint32_t address);
    void setDebugCallback(std::function<void(uint32_t, const std::string&)> callback);
    void dumpRegisters() const;
    void dumpJITCache() const;
    
    void enableJIT(bool enable);
    void flushJITCache();
    void setJITThreshold(uint32_t threshold);

    struct XMMRegister {
        uint8_t data[16];
    };
    std::array<XMMRegister, 8> xmmRegisters;

    struct FPUState {
        uint16_t controlWord;
        uint16_t statusWord;
        uint16_t tagWord;
        uint8_t lastOpcode;
        uint32_t lastIP;
        uint16_t lastCS;
        uint32_t lastDP;
        uint16_t lastDS;
        uint8_t st[8][10];
    } fpu;

    void setKernel(XboxKernel* kernelPtr);  
    
private:

    XboxMemory* memory;
    CpuState state;
    XboxKernel* kernel;  
    
    union {
        struct {
            uint32_t eax, ebx, ecx, edx;
            uint32_t esi, edi, esp, ebp;
        };
        uint32_t regs[8];
    };
    
    uint32_t eip;
    uint32_t eflags;
    uint16_t cs, ds, es, fs, gs, ss;
    uint32_t cr0, cr2, cr3, cr4;
    
    std::unordered_map<uint32_t, std::function<void()>> breakpoints;
    std::function<void(uint32_t, const std::string&)> debugCallback;
    
    struct JITBlock {
        uint32_t start_addr;
        uint32_t size;
        uint8_t* compiled_code;
    };
    
    std::unordered_map<uint32_t, JITBlock> jit_cache;
    std::unordered_map<uint32_t, uint32_t> executionCounts;
    void* jitCacheBase;
    size_t jitCacheUsed;
    bool jitEnabled;
    uint32_t jitThreshold;
    
    enum ALUOperation {
        ALU_ADD,
        ALU_SUB,
        ALU_MUL,
        ALU_DIV,
        ALU_AND,
        ALU_OR,
        ALU_XOR,
        ALU_SHL,
        ALU_SHR
    };
    
    void aluAdd(uint32_t& dest, uint32_t src);
    void aluSub(uint32_t& dest, uint32_t src);
    void aluMul(uint32_t& dest, uint32_t src);
    void updateFlags(uint32_t result, uint32_t a, uint32_t b, uint32_t operation);
    
    void decodeAndExecute(uint8_t opcode);
    
    void handleXboxSpecificOpcode();  
    void handleSyscall();             
    void handleMemoryProtection();    
    void handleCacheControl();        
    void handlePerformanceCounter();  
    
    void handleFPUOpcode(uint8_t opcode);
    void fpuGroup1(uint8_t modrm);
    void fpuGroup2(uint8_t modrm);
    void fpuGroup3(uint8_t modrm);
    void fpuGroup4(uint8_t modrm);
    void fpuGroup5(uint8_t modrm);
    void fpuGroup6(uint8_t modrm);
    void fpuGroup7(uint8_t modrm);
    void fpuGroup8(uint8_t modrm);
    void fpuFadd(uint8_t modrm);
    void fpuFsub(uint8_t modrm);
    void fpuFmul(uint8_t modrm);
    void fpuFdiv(uint8_t modrm);
    void fpuFcom(uint8_t modrm);
    void fpuFcomp(uint8_t modrm);
    void fpuFsubr(uint8_t modrm);
    void fpuFdivr(uint8_t modrm);
    
    void handleSSEOpcode(uint8_t opcode);
    void sseMovups(uint8_t modrm);
    void sseAddps(uint8_t modrm);
    void sseMulps(uint8_t modrm);
    void sseSubps(uint8_t modrm);
    void sseDivps(uint8_t modrm);
    
    uint32_t readOperand(uint8_t modrm);
    void writeOperand(uint8_t modrm, uint32_t value);
    
    void compileBlock(uint32_t start_addr);
    void executeCompiledBlock(JITBlock& block);
    bool isCommonOpcode(uint8_t opcode);
    void emitMOV(uint8_t*& code, uint8_t modrm);
    void emitADD(uint8_t*& code, uint8_t modrm);
    void emitRET(uint8_t*& code);
    void emitJMP(uint8_t*& code, int8_t offset);
    void emitXboxOpcode(uint8_t*& code, uint8_t op);  
    
    void mov_r32_rm32();
    void mov_rm32_r32();
    void add_r32_rm32();
    void add_rm32_r32();
    void jmp_rel8();
    void jmp_rel32();
    void ret();
};