#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <cstring>
#include <string>
#include <iostream>
#include <chrono>

class PCIBus;
class SMBus;
class MCPXDevice;
class SMCDevice;
class EEPROMDevice;
class NVNetDevice;
class NV2ADevice;
class USBDevice;
class MediaBoard;

typedef enum {
    Revision1_0,
    Revision1_1,
    Revision1_2,
    Revision1_3,
    Revision1_4,
    Revision1_5,
    Revision1_6,
    Retail = 0x00,
    DebugKit = 0x10,
    DebugKit_r1_2 = DebugKit | Revision1_2,
    Chihiro = 0x20,
    Chihiro_Type1 = Chihiro | Revision1_1,
    Chihiro_Type3 = Chihiro | Revision1_2,
} HardwareModel;

typedef enum {
    Conexant,
    Focus,
    XCalibur
} TVEncoder;

extern PCIBus* g_PCIBus;
extern SMBus* g_SMBus;
extern MCPXDevice* g_MCPX;
extern SMCDevice* g_SMC;
extern EEPROMDevice* g_EEPROM;
extern NVNetDevice* g_NVNet;
extern NV2ADevice* g_NV2A;
extern USBDevice* g_USB0;
extern MediaBoard* g_MediaBoard;

extern void InitXboxHardware(HardwareModel hardwareModel);

struct Memory {
    std::vector<uint8_t> data;
    size_t size() const { return data.size(); }
};

extern Memory memory;

#define GET_HW_REVISION(hardwareModel) (hardwareModel & 0x0F)
#define GET_HW_CONSOLE(hardwareModel) (hardwareModel & 0xF0)
#define IS_RETAIL(hardwareModel) (GET_HW_CONSOLE(hardwareModel) == Retail)
#define IS_DEVKIT(hardwareModel) (GET_HW_CONSOLE(hardwareModel) == DebugKit)
#define IS_CHIHIRO(hardwareModel) (GET_HW_CONSOLE(hardwareModel) == Chihiro)

extern bool loadISO(const std::string& isoUriString);

enum class InputType {
    KEYBOARD,
    MOUSE,
    JOYSTICK
};

struct InputEvent {
    InputType type;
    int key;
    int action;
    int mouseX, mouseY;
    int joystickId;
    float axisX, axisY;
};

extern std::mutex inputMutex;
extern std::queue<InputEvent> inputQueue;

inline void processMemory(uint32_t pc_local) {
    if (pc_local + 4 > memory.size()) {
        std::cerr << "Memory access out of bounds!" << std::endl;
        return;
    }

    uint32_t instruction;
    std::memcpy(&instruction, &memory.data[pc_local], sizeof(uint32_t));
}

extern void setupGraphics();
extern void processGPUCommands_Optimized();

extern uint32_t readMemory32(uint32_t address);
extern void writeMemory32(uint32_t address, uint32_t value);

extern void checkGLError(const std::string& context);

extern void cleanupResources();

#define EMUX86_EFLAG_CF 0x0001
#define EMUX86_EFLAG_PF 0x0004
#define EMUX86_EFLAG_AF 0x0010
#define EMUX86_EFLAG_ZF 0x0040
#define EMUX86_EFLAG_SF 0x0080
#define EMUX86_EFLAG_OF 0x0800

#define I_ADD 0x00
#define I_SUB 0x28
#define I_MOV 0x88
#define I_CMP 0x38
#define I_JMP 0xE9
#define I_CALL 0xE8
#define I_RET 0xC3
#define I_CPUID 0x0FA2
#define I_RDTSC 0x0F31

enum Register {
    R_EAX,
    R_ECX,
    R_EDX,
    R_EBX,
    R_ESP,
    R_EBP,
    R_ESI,
    R_EDI,
    R_EIP,
    R_EFLAGS
};

struct CPU {
    uint32_t registers[10];
    uint32_t pc;
    uint32_t sp;
    uint32_t status;
};

void EmuX86_SetFlags(CPU* cpu, uint32_t mask, uint32_t value) {
    cpu->registers[R_EFLAGS] ^= ((cpu->registers[R_EFLAGS] ^ value) & mask);
}

bool EmuX86_HasFlag(CPU* cpu, uint32_t flag) {
    return (cpu->registers[R_EFLAGS] & flag);
}

void EmuX86_Opcode_ADD(CPU* cpu, uint32_t src, uint32_t dest) {
    uint32_t result = dest + src;
    cpu->registers[R_EAX] = result;
    EmuX86_SetFlags(cpu, EMUX86_EFLAG_OF | EMUX86_EFLAG_SF | EMUX86_EFLAG_ZF | EMUX86_EFLAG_AF | EMUX86_EFLAG_PF | EMUX86_EFLAG_CF,
                    (((result ^ src ^ dest) >> 31) & EMUX86_EFLAG_OF) |
                    ((result >> 31) & EMUX86_EFLAG_SF) |
                    ((result == 0) ? EMUX86_EFLAG_ZF : 0) |
                    (((result ^ src ^ dest) >> 3) & EMUX86_EFLAG_AF) |
                    ((0x6996 >> ((result ^ (result >> 4)) & 0xF)) & EMUX86_EFLAG_PF) |
                    ((result < dest) ? EMUX86_EFLAG_CF : 0));
}

void EmuX86_Opcode_SUB(CPU* cpu, uint32_t src, uint32_t dest) {
    uint32_t result = dest - src;
    cpu->registers[R_EAX] = result;
    EmuX86_SetFlags(cpu, EMUX86_EFLAG_OF | EMUX86_EFLAG_SF | EMUX86_EFLAG_ZF | EMUX86_EFLAG_AF | EMUX86_EFLAG_PF | EMUX86_EFLAG_CF,
                    (((result ^ src ^ dest) >> 31) & EMUX86_EFLAG_OF) |
                    ((result >> 31) & EMUX86_EFLAG_SF) |
                    ((result == 0) ? EMUX86_EFLAG_ZF : 0) |
                    (((result ^ src ^ dest) >> 3) & EMUX86_EFLAG_AF) |
                    ((0x6996 >> ((result ^ (result >> 4)) & 0xF)) & EMUX86_EFLAG_PF) |
                    ((result > dest) ? EMUX86_EFLAG_CF : 0));
}

void EmuX86_Opcode_MOV(CPU* cpu, uint32_t src, uint32_t dest) {
    cpu->registers[dest] = src;
}

void EmuX86_Opcode_CMP(CPU* cpu, uint32_t src, uint32_t dest) {
    uint32_t result = dest - src;
    EmuX86_SetFlags(cpu, EMUX86_EFLAG_OF | EMUX86_EFLAG_SF | EMUX86_EFLAG_ZF | EMUX86_EFLAG_AF | EMUX86_EFLAG_PF | EMUX86_EFLAG_CF,
                    (((result ^ src ^ dest) >> 31) & EMUX86_EFLAG_OF) |
                    ((result >> 31) & EMUX86_EFLAG_SF) |
                    ((result == 0) ? EMUX86_EFLAG_ZF : 0) |
                    (((result ^ src ^ dest) >> 3) & EMUX86_EFLAG_AF) |
                    ((0x6996 >> ((result ^ (result >> 4)) & 0xF)) & EMUX86_EFLAG_PF) |
                    ((result > dest) ? EMUX86_EFLAG_CF : 0));
}

void EmuX86_Opcode_CPUID(CPU* cpu) {
    cpu->registers[R_EAX] = 0x00000001;
    cpu->registers[R_EBX] = 0x756E6547;
    cpu->registers[R_ECX] = 0x6C65746E;
    cpu->registers[R_EDX] = 0x49656E69;
}

void EmuX86_Opcode_RDTSC(CPU* cpu) {
    uint64_t tsc = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    cpu->registers[R_EAX] = static_cast<uint32_t>(tsc & 0xFFFFFFFF);
    cpu->registers[R_EDX] = static_cast<uint32_t>((tsc >> 32) & 0xFFFFFFFF);
}

uint32_t readMemory32(Memory* memory, uint32_t address) {
    if (address + 4 > memory->size()) {
        std::cerr << "Memory read out of bounds!" << std::endl;
        return 0;
    }
    uint32_t value;
    std::memcpy(&value, &memory->data[address], sizeof(uint32_t));
    return value;
}

void writeMemory32(Memory* memory, uint32_t address, uint32_t value) {
    if (address + 4 > memory->size()) {
        std::cerr << "Memory write out of bounds!" << std::endl;
        return;
    }
    std::memcpy(&memory->data[address], &value, sizeof(uint32_t));
}