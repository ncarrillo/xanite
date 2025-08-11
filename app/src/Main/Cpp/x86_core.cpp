#include "x86_core.h"
#include <android/log.h>
#include <stdexcept>
#include <arm_neon.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdio>

#define LOG_TAG "X86Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

constexpr size_t JIT_CACHE_SIZE = 16 * 1024 * 1024;
constexpr uint32_t MAX_BLOCK_SIZE = 256;
constexpr uint32_t JIT_THRESHOLD = 10;

X86Core::X86Core(XboxMemory* memory) :
    memory(memory),
    state(CpuState::Running),
    eip(0xFF000000),
    eflags(0x00000002),
    jitCacheBase(nullptr),
    jitCacheUsed(0),
    executionCounts(),
    fpu(),
    xmmRegisters(),
    jitEnabled(true),
    jitThreshold(JIT_THRESHOLD),
    traceEnabled(false),
    kernel(nullptr)  
{

    eax = ebx = ecx = edx = 0;
    esi = edi = esp = ebp = 0;
    cs = 0xF000;
    ds = es = fs = gs = ss = 0x0000;
    cr0 = 0x60000011;
    cr2 = cr3 = cr4 = 0;
    
    fpu.controlWord = 0x037F;
    fpu.statusWord = 0;
    fpu.tagWord = 0xFFFF;

    jitCacheBase = mmap(nullptr, JIT_CACHE_SIZE, 
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (jitCacheBase == MAP_FAILED) {
        LOGE("Failed to allocate JIT cache");
        jitEnabled = false;
    } else {
        LOGI("JIT cache allocated at %p (size: %zu bytes)", jitCacheBase, JIT_CACHE_SIZE);
    }
}

X86Core::~X86Core() {
    if (jitCacheBase) {
        munmap(jitCacheBase, JIT_CACHE_SIZE);
    }
}

void X86Core::reset() {
    eax = ebx = ecx = edx = 0;
    esi = edi = esp = ebp = 0;
    eip = 0xFF000000;
    eflags = 0x00000002;
    cs = 0xF000;
    ds = es = fs = gs = ss = 0x0000;
    cr0 = 0x60000011;
    cr2 = cr3 = cr4 = 0;
    state = CpuState::Running;

    for (auto& reg : xmmRegisters) {
        memset(reg.data, 0, sizeof(reg.data));
    }

    fpu.controlWord = 0x037F;
    fpu.statusWord = 0;
    fpu.tagWord = 0xFFFF;
    breakpoints.clear();
    jit_cache.clear();
    executionCounts.clear();

    if (jitCacheBase) {
        jitCacheUsed = 0;
    }
}

void X86Core::execute(uint32_t cycles) {
    uint32_t executed = 0;
    while (executed < cycles && state == CpuState::Running) {
        executeStep();
        executed++;
    }
}

void X86Core::executeStep() {
    if (state != CpuState::Running) return;

    if (memory->read8(eip) == 0x0F && memory->read8(eip + 1) == 0x3F) {
        handleXboxSpecificOpcode();
        return;
    }

    if (auto bp = breakpoints.find(eip); bp != breakpoints.end()) {
        bp->second();
        state = CpuState::DebugBreak;
        return;
    }

    executionCounts[eip]++;

    if (jitEnabled && executionCounts[eip] > jitThreshold) {
        if (auto block = jit_cache.find(eip); block != jit_cache.end()) {
            executeCompiledBlock(block->second);
            return;
        } else {
            compileBlock(eip);
            if (auto new_block = jit_cache.find(eip); new_block != jit_cache.end()) {
                executeCompiledBlock(new_block->second);
                return;
            }
        }
    }

    try {
        uint8_t opcode = memory->read8(eip++);
        
        switch (opcode) {
            case 0x90: break; 
            case 0xC3: ret(); return;
            case 0xEB: jmp_rel8(); return;
            default: decodeAndExecute(opcode);
        }
    } catch (const std::exception& e) {
        LOGE("CPU Exception: %s at 0x%08X", e.what(), eip);
        state = CpuState::Error;
    }
}

void X86Core::handleXboxSpecificOpcode() {
    
    eip += 2;
    
    uint8_t op = memory->read8(eip++);
    
    switch (op) {
        case 0x01: 
            handleSyscall();
            break;
        case 0x02:  
            handleMemoryProtection();
            break;
        case 0x03: 
            handleCacheControl();
            break;
        case 0x04:
            handlePerformanceCounter();
            break;
        default:
            LOGE("Unknown Xbox-specific opcode: 0x0F3F%02X at 0x%08X", op, eip - 1);
            state = CpuState::Error;
    }
}

void X86Core::handleSyscall() {
    if (!kernel) {
        LOGE("Kernel not set for syscall handling");
        state = CpuState::Error;
        return;
    }
    
    uint32_t call = eax;
    
    uint32_t args[4] = {ebx, ecx, edx, esi};
    
   uint32_t result = 0;  
    
    eax = result;
    
    if (traceEnabled) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Syscall 0x%08X -> 0x%08X", call, result);
        debugCallback(eip, buf);
    }
}

void X86Core::handleMemoryProtection() {
   
    LOGW("Xbox Memory Protection opcode not implemented");
}

void X86Core::handleCacheControl() {
   
    LOGW("Xbox Cache Control opcode not implemented");
}

void X86Core::handlePerformanceCounter() {
    
    LOGW("Xbox Performance Counter opcode not implemented");
}

void X86Core::compileBlock(uint32_t start_addr) {
    if (jitCacheUsed + MAX_BLOCK_SIZE > JIT_CACHE_SIZE) {
        LOGW("JIT cache full, flushing");
        flushJITCache();
    }

    JITBlock block;
    block.start_addr = start_addr;
    block.size = 0;
    block.compiled_code = reinterpret_cast<uint8_t*>(jitCacheBase) + jitCacheUsed;

    uint32_t current_addr = start_addr;
    uint8_t* code_ptr = block.compiled_code;
    bool end_block = false;

    while (current_addr < start_addr + MAX_BLOCK_SIZE && !end_block) {
        uint8_t opcode = memory->read8(current_addr);
        
        if (opcode == 0x0F && memory->read8(current_addr + 1) == 0x3F) {
            emitXboxOpcode(code_ptr, memory->read8(current_addr + 2));
            current_addr += 3;
            block.size += 3;
            continue;
        }
        
        switch (opcode) {
            case 0x8B: { 
                uint8_t modrm = memory->read8(current_addr + 1);
                emitMOV(code_ptr, modrm);
                current_addr += 2;
                block.size += 2;
                break;
            }
            case 0x89: { 
                uint8_t modrm = memory->read8(current_addr + 1);
                emitMOV(code_ptr, modrm);
                current_addr += 2;
                block.size += 2;
                break;
            }
            case 0x01: { 
                uint8_t modrm = memory->read8(current_addr + 1);
                emitADD(code_ptr, modrm);
                current_addr += 2;
                block.size += 2;
                break;
            }
            case 0x03: { 
                uint8_t modrm = memory->read8(current_addr + 1);
                emitADD(code_ptr, modrm);
                current_addr += 2;
                block.size += 2;
                break;
            }
            case 0xC3: { 
                emitRET(code_ptr);
                current_addr += 1;
                block.size += 1;
                end_block = true;
                break;
            }
            case 0xEB: { 
                emitJMP(code_ptr, memory->read8(current_addr + 1));
                current_addr += 2;
                block.size += 2;
                end_block = true;
                break;
            }
            default:
                end_block = true;
        }
    }

    if (block.size > 0) {
        jit_cache[start_addr] = block;
        jitCacheUsed += block.size;
        __builtin___clear_cache(reinterpret_cast<char*>(block.compiled_code), 
                               reinterpret_cast<char*>(block.compiled_code) + block.size);
        LOGD("Compiled block at 0x%08X (size: %u bytes)", start_addr, block.size);
    }
}

void X86Core::executeCompiledBlock(JITBlock& block) {
    typedef void (*JITFunction)();
    auto func = reinterpret_cast<JITFunction>(block.compiled_code);
    func();
}

void X86Core::decodeAndExecute(uint8_t opcode) {
    switch (opcode) {
        case 0x8B: mov_r32_rm32(); break;
        case 0x89: mov_rm32_r32(); break;
        case 0x01: add_rm32_r32(); break;
        case 0x03: add_r32_rm32(); break;
        case 0xE9: jmp_rel32(); break;
        case 0xEB: jmp_rel8(); break;
        case 0xC3: ret(); break;
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            handleFPUOpcode(opcode);
            break;
        case 0x0F:
            handleSSEOpcode(memory->read8(eip++));
            break;
        default:
            LOGE("Unknown opcode: 0x%02X at 0x%08X", opcode, eip-1);
            state = CpuState::Error;
    }
}

void X86Core::aluAdd(uint32_t& dest, uint32_t src) {
    uint32_t result = dest + src;
    eflags = (eflags & ~0x8D5) | 
             ((result == 0) << 6) |
             ((result >> 31) & 1) |
             ((result < dest) << 0) |
             ((((dest ^ src ^ 0x80000000) & (dest ^ result)) >> 30) & 0x4);
    dest = result;
}

void X86Core::aluSub(uint32_t& dest, uint32_t src) {
    uint32_t result = dest - src;
    eflags = (eflags & ~0x8D5) | 
             ((result == 0) << 6) |
             ((result >> 31) & 1) |
             ((dest < src) << 0) |
             ((((dest ^ src) & (dest ^ result)) >> 30) & 0x4);
    dest = result;
}

void X86Core::aluMul(uint32_t& dest, uint32_t src) {
    uint32x2_t a = vdup_n_u32(dest);
    uint32x2_t b = vdup_n_u32(src);
    uint64x2_t result = vmull_u32(a, b);
    uint64_t low = vgetq_lane_u64(result, 0);
    dest = low & 0xFFFFFFFF;
    uint32_t high = (low >> 32) & 0xFFFFFFFF;
    eflags = (eflags & ~0x801) | ((high != 0) << 0);
}

void X86Core::updateFlags(uint32_t result, uint32_t a, uint32_t b, uint32_t operation) {
    uint32_t flags = 0;
    flags |= (result == 0) << 6;
    flags |= (result & 0x80000000) >> 31;
    
    switch (operation) {
        case ALU_ADD: flags |= (result < a) << 0; break;
        case ALU_SUB: flags |= (a < b) << 0; break;
        case ALU_MUL: flags |= 0; break;
    }
    
    switch (operation) {
        case ALU_ADD: {
            uint32_t sign_a = a & 0x80000000;
            uint32_t sign_b = b & 0x80000000;
            uint32_t sign_r = result & 0x80000000;
            flags |= (sign_a == sign_b && sign_a != sign_r) ? 0x800 : 0;
            break;
        }
        case ALU_SUB: {
            uint32_t sign_a = a & 0x80000000;
            uint32_t sign_b = b & 0x80000000;
            uint32_t sign_r = result & 0x80000000;
            flags |= (sign_a != sign_b && sign_a != sign_r) ? 0x800 : 0;
            break;
        }
    }
    
    eflags = (eflags & ~0x8D5) | flags;
}

void X86Core::mov_r32_rm32() {
    uint8_t modrm = memory->read8(eip++);
    uint8_t reg = (modrm >> 3) & 7;
    uint32_t value = readOperand(modrm);
    setRegister(reg, value);
}

void X86Core::mov_rm32_r32() {
    uint8_t modrm = memory->read8(eip++);
    uint8_t reg = (modrm >> 3) & 7;
    uint32_t value = getRegister(reg);
    writeOperand(modrm, value);
}

void X86Core::add_rm32_r32() {
    uint8_t modrm = memory->read8(eip++);
    uint32_t src = getRegister((modrm >> 3) & 7);
    uint32_t dest = readOperand(modrm);
    aluAdd(dest, src);
    writeOperand(modrm, dest);
}

void X86Core::add_r32_rm32() {
    uint8_t modrm = memory->read8(eip++);
    uint8_t reg = (modrm >> 3) & 7;
    uint32_t src = readOperand(modrm);
    uint32_t dest = getRegister(reg);
    aluAdd(dest, src);
    setRegister(reg, dest);
}

void X86Core::jmp_rel8() {
    int8_t offset = memory->read8(eip++);
    eip += offset;
}

void X86Core::jmp_rel32() {
    int32_t offset = memory->read32(eip);
    eip += 4 + offset;
}

void X86Core::ret() {
    eip = memory->read32(esp);
    esp += 4;
}

void X86Core::handleFPUOpcode(uint8_t opcode) {
    uint8_t modrm = memory->read8(eip++);
    switch (opcode) {
        case 0xD8: fpuGroup1(modrm); break;
        case 0xD9: fpuGroup2(modrm); break;
        case 0xDA: fpuGroup3(modrm); break;
        case 0xDB: fpuGroup4(modrm); break;
        case 0xDC: fpuGroup5(modrm); break;
        case 0xDD: fpuGroup6(modrm); break;
        case 0xDE: fpuGroup7(modrm); break;
        case 0xDF: fpuGroup8(modrm); break;
    }
}

void X86Core::fpuGroup1(uint8_t modrm) {
    uint8_t op = (modrm >> 3) & 7;
    switch (op) {
        case 0: fpuFadd(modrm); break;
        case 1: fpuFmul(modrm); break;
        case 2: fpuFcom(modrm); break;
        case 3: fpuFcomp(modrm); break;
        case 4: fpuFsub(modrm); break;
        case 5: fpuFsubr(modrm); break;
        case 6: fpuFdiv(modrm); break;
        case 7: fpuFdivr(modrm); break;
    }
}

void X86Core::fpuGroup2(uint8_t) { LOGW("FPU Group2 instruction not implemented"); }
void X86Core::fpuGroup3(uint8_t) { LOGW("FPU Group3 instruction not implemented"); }
void X86Core::fpuGroup4(uint8_t) { LOGW("FPU Group4 instruction not implemented"); }
void X86Core::fpuGroup5(uint8_t) { LOGW("FPU Group5 instruction not implemented"); }
void X86Core::fpuGroup6(uint8_t) { LOGW("FPU Group6 instruction not implemented"); }
void X86Core::fpuGroup7(uint8_t) { LOGW("FPU Group7 instruction not implemented"); }
void X86Core::fpuGroup8(uint8_t) { LOGW("FPU Group8 instruction not implemented"); }
void X86Core::fpuFadd(uint8_t) { LOGW("FPU FADD instruction not implemented"); }
void X86Core::fpuFmul(uint8_t) { LOGW("FPU FMUL instruction not implemented"); }
void X86Core::fpuFcom(uint8_t) { LOGW("FPU FCOM instruction not implemented"); }
void X86Core::fpuFcomp(uint8_t) { LOGW("FPU FCOMP instruction not implemented"); }
void X86Core::fpuFsub(uint8_t) { LOGW("FPU FSUB instruction not implemented"); }
void X86Core::fpuFsubr(uint8_t) { LOGW("FPU FSUBR instruction not implemented"); }
void X86Core::fpuFdiv(uint8_t) { LOGW("FPU FDIV instruction not implemented"); }
void X86Core::fpuFdivr(uint8_t) { LOGW("FPU FDIVR instruction not implemented"); }

void X86Core::handleSSEOpcode(uint8_t opcode) {
    uint8_t modrm = memory->read8(eip++);
    switch (opcode) {
        case 0x10: sseMovups(modrm); break;
        case 0x58: sseAddps(modrm); break;
        case 0x59: sseMulps(modrm); break;
        case 0x5C: sseSubps(modrm); break;
        case 0x5E: sseDivps(modrm); break;
        default:
            LOGE("Unknown SSE opcode: 0x0F%02X", opcode);
            state = CpuState::Error;
    }
}

void X86Core::sseMovups(uint8_t modrm) {
    uint8_t reg = (modrm >> 3) & 7;
    uint32_t addr = readOperand(modrm);
    for (int i = 0; i < 4; i++) {
        uint32_t val = memory->read32(addr + i*4);
        memcpy(&xmmRegisters[reg].data[i*4], &val, 4);
    }
}

void X86Core::sseAddps(uint8_t modrm) {
    uint8_t reg1 = (modrm >> 3) & 7;
    uint8_t reg2 = modrm & 7;
    float32x4_t a = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data));
    float32x4_t b = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg2].data));
    float32x4_t result = vaddq_f32(a, b);
    vst1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data), result);
}

void X86Core::sseMulps(uint8_t modrm) {
    uint8_t reg1 = (modrm >> 3) & 7;
    uint8_t reg2 = modrm & 7;
    float32x4_t a = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data));
    float32x4_t b = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg2].data));
    float32x4_t result = vmulq_f32(a, b);
    vst1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data), result);
}

void X86Core::sseSubps(uint8_t modrm) {
    uint8_t reg1 = (modrm >> 3) & 7;
    uint8_t reg2 = modrm & 7;
    float32x4_t a = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data));
    float32x4_t b = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg2].data));
    float32x4_t result = vsubq_f32(a, b);
    vst1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data), result);
}

void X86Core::sseDivps(uint8_t modrm) {
    uint8_t reg1 = (modrm >> 3) & 7;
    uint8_t reg2 = modrm & 7;
    float32x4_t a = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data));
    float32x4_t b = vld1q_f32(reinterpret_cast<float*>(xmmRegisters[reg2].data));
    float32x4_t result = vdivq_f32(a, b);
    vst1q_f32(reinterpret_cast<float*>(xmmRegisters[reg1].data), result);
}

void X86Core::emitMOV(uint8_t*& code, uint8_t modrm) {
    *code++ = 0x8B;
    *code++ = modrm;
}

void X86Core::emitADD(uint8_t*& code, uint8_t modrm) {
    *code++ = 0x01;
    *code++ = modrm;
}

void X86Core::emitRET(uint8_t*& code) {
    *code++ = 0xC3;
}

void X86Core::emitJMP(uint8_t*& code, int8_t offset) {
    *code++ = 0xEB;
    *code++ = offset;
}

void X86Core::emitXboxOpcode(uint8_t*& code, uint8_t op) {
    
    *code++ = 0x0F;
    *code++ = 0x3F;
    *code++ = op;
}

uint32_t X86Core::readOperand(uint8_t modrm) {
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t rm = modrm & 7;
    
    switch (mod) {
        case 0: return memory->read32(getRegister(rm));
        case 1: return memory->read32(getRegister(rm) + (int8_t)memory->read8(eip++));
        case 2: return memory->read32(getRegister(rm) + memory->read32(eip));
        case 3: return getRegister(rm);
        default: return 0;
    }
}

void X86Core::writeOperand(uint8_t modrm, uint32_t value) {
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t rm = modrm & 7;
    
    switch (mod) {
        case 0: memory->write32(getRegister(rm), value); break;
        case 1: memory->write32(getRegister(rm) + (int8_t)memory->read8(eip++), value); break;
        case 2: memory->write32(getRegister(rm) + memory->read32(eip), value); break;
        case 3: setRegister(rm, value); break;
    }
}

void X86Core::flushJITCache() {
    jit_cache.clear();
    jitCacheUsed = 0;
    LOGI("JIT cache flushed");
}

uint32_t X86Core::getRegister(uint8_t reg) const {
    switch (reg) {
        case 0: return eax;
        case 1: return ecx;
        case 2: return edx;
        case 3: return ebx;
        case 4: return esp;
        case 5: return ebp;
        case 6: return esi;
        case 7: return edi;
        default: return 0;
    }
}

void X86Core::setRegister(uint8_t reg, uint32_t value) {
    switch (reg) {
        case 0: eax = value; break;
        case 1: ecx = value; break;
        case 2: edx = value; break;
        case 3: ebx = value; break;
        case 4: esp = value; break;
        case 5: ebp = value; break;
        case 6: esi = value; break;
        case 7: edi = value; break;
    }
}

void X86Core::dumpRegisters() const {
    LOGI("EAX: 0x%08X EBX: 0x%08X ECX: 0x%08X EDX: 0x%08X", eax, ebx, ecx, edx);
    LOGI("ESI: 0x%08X EDI: 0x%08X ESP: 0x%08X EBP: 0x%08X", esi, edi, esp, ebp);
    LOGI("EIP: 0x%08X EFLAGS: 0x%08X", eip, eflags);
    LOGI("CS: 0x%04X DS: 0x%04X ES: 0x%04X FS: 0x%04X GS: 0x%04X SS: 0x%04X", cs, ds, es, fs, gs, ss);
}

void X86Core::dumpJITCache() const {
    LOGI("JIT Cache Usage: %zu/%zu bytes (%.1f%%)", 
         jitCacheUsed, JIT_CACHE_SIZE, 
         (float)jitCacheUsed/JIT_CACHE_SIZE*100);
    LOGI("Compiled Blocks: %zu", jit_cache.size());
}

void X86Core::handleInterrupt(unsigned char interruptNumber) {
    
    LOGW("Interrupt %d handled", interruptNumber);
}

void X86Core::setKernel(XboxKernel* kernelPtr) {
    kernel = kernelPtr;
}

void X86Core::enableTracing(bool enabled) {
    traceEnabled = enabled;
}

void X86Core::enableJIT(bool enable) {
    jitEnabled = enable;
}

void X86Core::setJITThreshold(uint32_t threshold) {
    jitThreshold = threshold;
}

void X86Core::setDebugCallback(std::function<void(uint32_t, const std::string&)> callback) {
    debugCallback = callback;
}

void X86Core::setPC(uint32_t pc) {
    eip = pc;
}