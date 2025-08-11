#include "nv2a_renderer.h"
#include "xbox_memory.h"
#include <cmath>
#include <cstring>
#include <android/log.h>
#include <arm_neon.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>

#define LOG_TAG "NV2ARenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


NV2ARenderer::NV2ARenderer(XboxMemory* memory) : 
    memory(memory),
    framebuffer(FB_SIZE, 0xFF000000),
    depthBuffer(FB_SIZE, 1.0f),
    textureMemory(TEXTURE_MEMORY, 0),
    vertexBuffer(),
    renderThread(nullptr),
    vsyncEnabled(true),   
    currentState(GpuState::Ready),
    currentPrimitive(PrimitiveType::Triangles),
    currentTexture(0),
    depthTestEnabled(false),
    alphaBlendEnabled(false),
    textureFilteringEnabled(true),
    frameCounter(0)
{
    vertexBuffer.reserve(MAX_VERTICES * 2);
  
    for (auto& unit : textureUnits) {
        unit.width = 0;
        unit.height = 0;
        unit.format = 0;
        unit.address = 0;
        unit.pitch = 0;
        unit.mipLevels = 1;
        unit.swizzled = false;
    }
    
    reset();
    
    renderThread = new std::thread(&NV2ARenderer::renderThreadFunc, this);
}

NV2ARenderer::NV2ARenderer() : NV2ARenderer(nullptr) {}

NV2ARenderer::~NV2ARenderer() {
    if (renderThread) {
        {
            std::lock_guard<std::mutex> lock(renderMutex);
            currentState = GpuState::Error;
            renderCond.notify_all();
        }
        renderThread->join();
        delete renderThread;
    }
}

void NV2ARenderer::renderThreadFunc() {
    while (true) {
        std::unique_lock<std::mutex> lock(renderMutex);
        renderCond.wait(lock, [this] {
            return !cmdState.fifoEmpty || currentState == GpuState::Error;
        });
        
        if (currentState == GpuState::Error) break;
        
        auto frameStart = std::chrono::high_resolution_clock::now();
        
        processCommandBuffer();
       
        processVertices();
        
        frameCounter++;
        vertexBuffer.clear();
        
        if (vsyncEnabled) {
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
            if (elapsed.count() < 16) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16 - elapsed.count()));
            }
        }
        
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }
}

void NV2ARenderer::renderFrame() {
    if (!renderThread) {
        std::lock_guard<std::mutex> lock(renderMutex);
        processCommandBuffer();
        processVertices();
        vertexBuffer.clear();
    } else {
        std::lock_guard<std::mutex> lock(renderMutex);
        renderCond.notify_one();
    }
}

void NV2ARenderer::reset() {
    std::lock_guard<std::mutex> lock(renderMutex);
    
    memset(registers.data(), 0, registers.size() * sizeof(uint32_t));
    
    for (auto& unit : textureUnits) {
        unit.width = 0;
        unit.height = 0;
        unit.format = 0;
        unit.address = 0;
    }
   
    cmdState.pc = 0;
    cmdState.put = 0;
    cmdState.get = 0;
    cmdState.fifoEmpty = true;
   
    dmaState.source = 0;
    dmaState.dest = 0;
    dmaState.size = 0;
    dmaState.active = false;
    
    clipRect = {0, 0, FB_WIDTH, FB_HEIGHT};
    
    currentState = GpuState::Ready;
    currentPrimitive = PrimitiveType::Triangles;
    currentTexture = 0;
    
    clearFramebuffer(0xFF000000);
    std::fill(depthBuffer.begin(), depthBuffer.end(), 1.0f);
}

void NV2ARenderer::processCommandBuffer() {
    while (!cmdState.fifoEmpty && cmdState.pc < cmdState.put) {
        uint32_t command = registers[cmdState.pc / 4];
        cmdState.pc += 4;
        const uint8_t opcode = command & 0xFF;
        
        switch (opcode) {
            case 0x00: 
                break;
                
            case 0x20: 
                handlePrimitive(command);
                break;
                
            case 0x40: 
            case 0x60: 
                handleVertexData(command);
                break;
                
            case 0x80:
                handleTextureUpload(command);
                break;
                
            case 0xA0: 
                handleRegisterWrite(command >> 8, registers[cmdState.pc / 4]);
                cmdState.pc += 4;
                break;
                
            case 0xE0: 
                handleSpecialCommand(command);
                break;
                
            default:
                if (opcode >= 0xE0) {
                    handleRegisterWrite(opcode, command >> 8);
                }
                break;
        }
    }
    
    cmdState.fifoEmpty = (cmdState.pc >= cmdState.put);
}

void NV2ARenderer::processVertices() {
    switch (currentPrimitive) {
        case PrimitiveType::Points:
            processPoints();
            break;
            
        case PrimitiveType::Lines:
            processLines();
            break;
            
        case PrimitiveType::LineStrip:
            processLineStrip();
            break;
            
        case PrimitiveType::Triangles:
            processTriangles();
            break;
            
        case PrimitiveType::TriangleStrip:
            processTriangleStrip();
            break;
            
        case PrimitiveType::TriangleFan:
            processTriangleFan();
            break;
            
        case PrimitiveType::Quads:
            processQuads();
            break;
            
        case PrimitiveType::QuadStrip:
            processQuadStrip();
            break;
            
        case PrimitiveType::Polygon:
            processPolygon();
            break;
    }
}

void NV2ARenderer::drawTriangleNEON(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    int x0 = static_cast<int>(v0.x * FB_WIDTH);
    int y0 = static_cast<int>(v0.y * FB_HEIGHT);
    int x1 = static_cast<int>(v1.x * FB_WIDTH);
    int y1 = static_cast<int>(v1.y * FB_HEIGHT);
    int x2 = static_cast<int>(v2.x * FB_WIDTH);
    int y2 = static_cast<int>(v2.y * FB_HEIGHT);

    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }

    int minX = std::max(std::min({x0, x1, x2}), clipRect.left);
    int maxX = std::min(std::max({x0, x1, x2}), clipRect.right);
    int minY = std::max(std::min({y0, y1, y2}), clipRect.top);
    int maxY = std::min(std::max({y0, y1, y2}), clipRect.bottom);

    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabs(area) < 0.5f) return;
    float inv_area = 1.0f / area;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float w0 = (x1 - x) * (y2 - y) - (x2 - x) * (y1 - y);
            float w1 = (x2 - x) * (y0 - y) - (x0 - x) * (y2 - y);
            float w2 = (x0 - x) * (y1 - y) - (x1 - x) * (y0 - y);

            w0 *= inv_area;
            w1 *= inv_area;
            w2 *= inv_area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float depth = w0 * v0.z + w1 * v1.z + w2 * v2.z;

                if (!depthTestEnabled || depthTest(x, y, depth)) {
                    float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
                    float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;

                    uint32_t tex_color = sampleTexture(u, v, currentTexture);
                    uint32_t final_color = blendColors(v0.color, tex_color);

                    framebuffer[y * FB_WIDTH + x] = final_color;
                    if (depthTestEnabled) {
                        depthBuffer[y * FB_WIDTH + x] = depth;
                    }
                }
            }
        }
    }
}

uint32_t NV2ARenderer::sampleTexture(float u, float v, uint32_t texUnit) {
    if (texUnit >= textureUnits.size()) return 0xFFFFFFFF;

    const auto& tex = textureUnits[texUnit];
    if (tex.width == 0 || tex.height == 0) return 0xFFFFFFFF;

    u = fmod(u, 1.0f);
    v = fmod(v, 1.0f);
    if (u < 0) u += 1.0f;
    if (v < 0) v += 1.0f;

    float x = u * (tex.width - 1);
    float y = v * (tex.height - 1);

    if (textureFilteringEnabled) {
        return sampleTextureBilinear(x, y, tex);
    } else {
        return sampleTextureNearest(x, y, tex);
    }
}

uint32_t NV2ARenderer::sampleTextureNearest(float x, float y, const TextureInfo& tex) {
    uint32_t xi = static_cast<uint32_t>(x + 0.5f);
    uint32_t yi = static_cast<uint32_t>(y + 0.5f);
    
    xi = std::min(xi, tex.width - 1);
    yi = std::min(yi, tex.height - 1);
    
    uint32_t addr = tex.address + yi * tex.pitch + xi * 4;
    return *reinterpret_cast<uint32_t*>(&textureMemory[addr]);
}

uint32_t NV2ARenderer::sampleTextureBilinear(float x, float y, const TextureInfo& tex) {
    uint32_t x0 = static_cast<uint32_t>(x);
    uint32_t y0 = static_cast<uint32_t>(y);
    uint32_t x1 = std::min(x0 + 1, tex.width - 1);
    uint32_t y1 = std::min(y0 + 1, tex.height - 1);
    
    float fx = x - x0;
    float fy = y - y0;
    
    uint32_t addr00 = tex.address + y0 * tex.pitch + x0 * 4;
    uint32_t addr01 = tex.address + y0 * tex.pitch + x1 * 4;
    uint32_t addr10 = tex.address + y1 * tex.pitch + x0 * 4;
    uint32_t addr11 = tex.address + y1 * tex.pitch + x1 * 4;
    
    uint32_t c00 = *reinterpret_cast<uint32_t*>(&textureMemory[addr00]);
    uint32_t c01 = *reinterpret_cast<uint32_t*>(&textureMemory[addr01]);
    uint32_t c10 = *reinterpret_cast<uint32_t*>(&textureMemory[addr10]);
    uint32_t c11 = *reinterpret_cast<uint32_t*>(&textureMemory[addr11]);
    
    return bilinearInterpolate(c00, c01, c10, c11, fx, fy);
}

uint32_t NV2ARenderer::bilinearInterpolate(uint32_t c00, uint32_t c01, uint32_t c10, uint32_t c11, float fx, float fy) {
    float r00 = (c00 >> 16) & 0xFF;
    float g00 = (c00 >> 8) & 0xFF;
    float b00 = c00 & 0xFF;
    float a00 = (c00 >> 24) & 0xFF;
    
    float r01 = (c01 >> 16) & 0xFF;
    float g01 = (c01 >> 8) & 0xFF;
    float b01 = c01 & 0xFF;
    float a01 = (c01 >> 24) & 0xFF;
    
    float r10 = (c10 >> 16) & 0xFF;
    float g10 = (c10 >> 8) & 0xFF;
    float b10 = c10 & 0xFF;
    float a10 = (c10 >> 24) & 0xFF;
    
    float r11 = (c11 >> 16) & 0xFF;
    float g11 = (c11 >> 8) & 0xFF;
    float b11 = c11 & 0xFF;
    float a11 = (c11 >> 24) & 0xFF;
    
    float r0 = r00 * (1 - fx) + r01 * fx;
    float g0 = g00 * (1 - fx) + g01 * fx;
    float b0 = b00 * (1 - fx) + b01 * fx;
    float a0 = a00 * (1 - fx) + a01 * fx;
    
    float r1 = r10 * (1 - fx) + r11 * fx;
    float g1 = g10 * (1 - fx) + g11 * fx;
    float b1 = b10 * (1 - fx) + b11 * fx;
    float a1 = a10 * (1 - fx) + a11 * fx;
    
    float r = r0 * (1 - fy) + r1 * fy;
    float g = g0 * (1 - fy) + g1 * fy;
    float b = b0 * (1 - fy) + b1 * fy;
    float a = a0 * (1 - fy) + a1 * fy;
    
    return (static_cast<uint32_t>(a) << 24) | 
           (static_cast<uint32_t>(r) << 16) | 
           (static_cast<uint32_t>(g) << 8) | 
           static_cast<uint32_t>(b);
}

uint32_t NV2ARenderer::blendPixels(uint32_t src, uint32_t dst) {
    float src_a = ((src >> 24) & 0xFF) / 255.0f;
    float src_r = ((src >> 16) & 0xFF) / 255.0f;
    float src_g = ((src >> 8) & 0xFF) / 255.0f;
    float src_b = (src & 0xFF) / 255.0f;
    
    float dst_a = ((dst >> 24) & 0xFF) / 255.0f;
    float dst_r = ((dst >> 16) & 0xFF) / 255.0f;
    float dst_g = ((dst >> 8) & 0xFF) / 255.0f;
    float dst_b = (dst & 0xFF) / 255.0f;
    
    float a = src_a + dst_a * (1 - src_a);
    if (a < 0.001f) return 0;
    
    float r = (src_r * src_a + dst_r * dst_a * (1 - src_a)) / a;
    float g = (src_g * src_a + dst_g * dst_a * (1 - src_a)) / a;
    float b = (src_b * src_a + dst_b * dst_a * (1 - src_a)) / a;
    
    return (static_cast<uint32_t>(a * 255) << 24) |
           (static_cast<uint32_t>(r * 255) << 16) |
           (static_cast<uint32_t>(g * 255) << 8) |
           static_cast<uint32_t>(b * 255);
}

bool NV2ARenderer::depthTest(int x, int y, float depth) {
    if (x < 0 || static_cast<uint32_t>(x) >= FB_WIDTH || 
    y < 0 || static_cast<uint32_t>(y) >= FB_HEIGHT) return false;
    
    float current_depth = depthBuffer[y * FB_WIDTH + x];
    if (depth <= current_depth) {
        depthBuffer[y * FB_WIDTH + x] = depth;
        return true;
    }
    return false;
}

void NV2ARenderer::clearFramebuffer(uint32_t color) {
    uint32x4_t color_vec = vdupq_n_u32(color);
    uint32_t* ptr = framebuffer.data();
    
    for (size_t i = 0; i < FB_SIZE; i += 4) {
        vst1q_u32(ptr + i, color_vec);
    }
    
    float32x4_t depth_vec = vdupq_n_f32(1.0f);
    float* depth_ptr = depthBuffer.data();
    for (size_t i = 0; i < FB_SIZE; i += 4) {
        vst1q_f32(depth_ptr + i, depth_vec);
    }
}

void NV2ARenderer::uploadTexture(uint32_t dest, const uint8_t* src, uint32_t size) {
    if (dest + size > TEXTURE_MEMORY) {
        LOGE("Texture upload out of bounds");
        return;
    }
    
    if ((dest % 16 == 0) && (reinterpret_cast<uintptr_t>(src) % 16 == 0)) {
        uint32x4_t* dst_ptr = reinterpret_cast<uint32x4_t*>(&textureMemory[dest]);
        const uint32x4_t* src_ptr = reinterpret_cast<const uint32x4_t*>(src);
        
        for (uint32_t i = 0; i < size / 16; i++) {
            uint32x4_t data = vld1q_u32(reinterpret_cast<const uint32_t*>(&src_ptr[i]));
            vst1q_u32(reinterpret_cast<uint32_t*>(&dst_ptr[i]), data);
        }
    } else {
        memcpy(&textureMemory[dest], src, size);
    }
}

void NV2ARenderer::handlePrimitive(uint32_t command) {
    PrimitiveType type = static_cast<PrimitiveType>((command >> 8) & 0xFF);
    currentPrimitive = type;
    
    switch (type) {
        case PrimitiveType::Points:
        case PrimitiveType::Lines:
        case PrimitiveType::Triangles:
            break;
            
        default:
            LOGE("Unsupported primitive type: %d", static_cast<int>(type));
            break;
    }
}

void NV2ARenderer::handleVertexData(uint32_t command) {
    uint32_t count = (command >> 16) & 0xFF;
    uint32_t format = command & 0xFFFF;
    
    for (uint32_t i = 0; i < count; i++) {
        Vertex v;
        
        if (format & 0x01) { 
            v.x = *reinterpret_cast<const float*>(&registers[cmdState.pc / 4]);
            cmdState.pc += 4;
            v.y = *reinterpret_cast<const float*>(&registers[cmdState.pc / 4]);
            cmdState.pc += 4;
            v.z = *reinterpret_cast<const float*>(&registers[cmdState.pc / 4]);
            cmdState.pc += 4;
        }
        
        if (format & 0x02) { 
            v.u = *reinterpret_cast<const float*>(&registers[cmdState.pc / 4]);
            cmdState.pc += 4;
            v.v = *reinterpret_cast<const float*>(&registers[cmdState.pc / 4]);
            cmdState.pc += 4;
        }
        
        if (format & 0x04) { 
            v.color = registers[cmdState.pc / 4];
            cmdState.pc += 4;
        }
        
        vertexBuffer.push_back(v);
    }
}

void NV2ARenderer::drawLineNEON(const Vertex& v0, const Vertex& v1) {
   
    drawLine(v0, v1);
}

void NV2ARenderer::processLineStrip() {
    if (vertexBuffer.size() < 2) return;
    
    for (size_t i = 1; i < vertexBuffer.size(); ++i) {
        drawLine(vertexBuffer[i-1], vertexBuffer[i]);
    }
}

void NV2ARenderer::processTriangleStrip() {
    if (vertexBuffer.size() < 3) return;
    
    for (size_t i = 2; i < vertexBuffer.size(); ++i) {
        drawTriangle(vertexBuffer[i-2], vertexBuffer[i-1], vertexBuffer[i]);
    }
}

void NV2ARenderer::processQuadStrip() {
    if (vertexBuffer.size() < 4) return;
    
    for (size_t i = 3; i < vertexBuffer.size(); i += 2) {
        drawQuad(vertexBuffer[i-3], vertexBuffer[i-2], vertexBuffer[i-1], vertexBuffer[i]);
    }
}

void NV2ARenderer::processTriangleFan() {
    if (vertexBuffer.size() < 3) return;
    
    const Vertex& center = vertexBuffer[0];
    for (size_t i = 2; i < vertexBuffer.size(); ++i) {
        drawTriangle(center, vertexBuffer[i-1], vertexBuffer[i]);
    }
}

void NV2ARenderer::processQuads() {
    if (vertexBuffer.size() < 4) return;
    
    for (size_t i = 0; i < vertexBuffer.size(); i += 4) {
        if (i + 3 >= vertexBuffer.size()) break;
        drawQuad(vertexBuffer[i], vertexBuffer[i+1], vertexBuffer[i+2], vertexBuffer[i+3]);
    }
}



void NV2ARenderer::processPolygon() {
    if (vertexBuffer.size() < 3) return;
    
    const Vertex& first = vertexBuffer[0];
    for (size_t i = 2; i < vertexBuffer.size(); ++i) {
        drawTriangle(first, vertexBuffer[i-1], vertexBuffer[i]);
    }
}

uint32_t NV2ARenderer::blendColors(uint32_t color1, uint32_t color2) {
    uint8_t a1 = (color1 >> 24) & 0xFF;
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t a2 = (color2 >> 24) & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    uint8_t a = (a1 + a2) / 2;
    uint8_t r = (r1 + r2) / 2;
    uint8_t g = (g1 + g2) / 2;
    uint8_t b = (b1 + b2) / 2;
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void NV2ARenderer::handleTextureUpload(uint32_t command) {
    (void)command; 
    uint32_t dest = registers[cmdState.pc / 4];
    cmdState.pc += 4;
    uint32_t size = registers[cmdState.pc / 4];
    cmdState.pc += 4;
    
    if (dest + size > TEXTURE_MEMORY) {
        LOGE("Texture upload out of bounds");
        return;
    }
    
    if (memory) {
        const uint8_t* src = memory->getRamPointer() + dest;
        uploadTexture(dest, src, size);
    } else {
        LOGE("No memory assigned for texture upload");
    }
}

void NV2ARenderer::handleRegisterWrite(uint32_t reg, uint32_t value) {
    if (reg >= registers.size()) {
        LOGE("Register write out of bounds: 0x%04X", reg);
        return;
    }
    
    registers[reg] = value;
    
    switch (reg) {
        case 0x1000: 
            depthTestEnabled = (value & 1);
            break;
            
        case 0x1004: 
            alphaBlendEnabled = (value & 1);
            break;
            
        case 0x2000: 
            currentTexture = value % textureUnits.size();
            break;
            
        case 0x3000: 
            textureFilteringEnabled = (value & 1);
            break;
    }
}

void NV2ARenderer::processPoints() {
    for (const auto& v : vertexBuffer) {
        int x = static_cast<int>(v.x * FB_WIDTH);
        int y = static_cast<int>(v.y * FB_HEIGHT);
        
        if (x >= clipRect.left && x < clipRect.right && 
            y >= clipRect.top && y < clipRect.bottom) {
            framebuffer[y * FB_WIDTH + x] = v.color;
        }
    }
}

void NV2ARenderer::processLines() {
    for (size_t i = 0; i + 1 < vertexBuffer.size(); i += 2) {
        drawLineNEON(vertexBuffer[i], vertexBuffer[i+1]);
    }
}

void NV2ARenderer::processTriangles() {
    for (size_t i = 0; i + 2 < vertexBuffer.size(); i += 3) {
        drawTriangleNEON(vertexBuffer[i], vertexBuffer[i+1], vertexBuffer[i+2]);
    }
}

void NV2ARenderer::drawPoint(const Vertex& v) {
    int x = static_cast<int>(v.x * FB_WIDTH);
    int y = static_cast<int>(v.y * FB_HEIGHT);
    if (x >= 0 && x < static_cast<int>(FB_WIDTH) && y >= 0 && y < static_cast<int>(FB_HEIGHT)) {
        framebuffer[y * FB_WIDTH + x] = v.color;
    }
}

void NV2ARenderer::drawLine(const Vertex& v0, const Vertex& v1) {

    int x0 = static_cast<int>(v0.x * FB_WIDTH);
    int y0 = static_cast<int>(v0.y * FB_HEIGHT);
    int x1 = static_cast<int>(v1.x * FB_WIDTH);
    int y1 = static_cast<int>(v1.y * FB_HEIGHT);
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        if (x0 >= clipRect.left && x0 < clipRect.right && 
            y0 >= clipRect.top && y0 < clipRect.bottom) {
            framebuffer[y0 * FB_WIDTH + x0] = v0.color;
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void NV2ARenderer::drawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    
    drawTriangleNEON(v0, v1, v2);
}

void NV2ARenderer::drawQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {

    drawTriangle(v0, v1, v2);
    drawTriangle(v2, v3, v0);
}

void NV2ARenderer::handleSpecialCommand(uint32_t command) {

    LOGE("Unhandled special command: 0x%08X", command);
}

void NV2ARenderer::logDebug(const std::string& message) {
    if (debugCallback) {
        debugCallback(message);
    }
}

void NV2ARenderer::updateDMA() {

}

void NV2ARenderer::checkFifoStatus() {
    
}

void NV2ARenderer::setupDefaultState() {

}

void NV2ARenderer::downloadTexture(uint8_t*, uint32_t, uint32_t) {
    
}

void NV2ARenderer::swizzleTexture(uint8_t*, const uint8_t*, uint32_t, uint32_t) {
    
}

void NV2ARenderer::deswizzleTexture(uint8_t*, const uint8_t*, uint32_t, uint32_t) {
    
}

void NV2ARenderer::processDMA() {
    
}

uint32_t NV2ARenderer::readRegister(uint32_t addr) {
    if (addr < registers.size() * sizeof(uint32_t)) {
        return registers[addr / 4];
    }
    return 0;
}

void NV2ARenderer::writeRegister(uint32_t addr, uint32_t value) {
    if (addr < registers.size() * sizeof(uint32_t)) {
        registers[addr / 4] = value;
    }
}

void NV2ARenderer::enableDepthTest(bool enable) {
    depthTestEnabled = enable;
}

void NV2ARenderer::enableAlphaBlending(bool enable) {
    alphaBlendEnabled = enable;
}

void NV2ARenderer::setClipRect(int left, int top, int right, int bottom) {
    clipRect = {left, top, right, bottom};
}

const uint32_t* NV2ARenderer::getFramebuffer() const {
    return framebuffer.data();
}

uint32_t NV2ARenderer::getWidth() const {
    return FB_WIDTH;
}

uint32_t NV2ARenderer::getHeight() const {
    return FB_HEIGHT;
}

void NV2ARenderer::setDebugCallback(std::function<void(const std::string&)> callback) {
    debugCallback = callback;
}

NV2ARenderer::GpuState NV2ARenderer::getState() const {
    return currentState;
}
