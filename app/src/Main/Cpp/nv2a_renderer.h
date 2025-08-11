#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <arm_neon.h>
#include <algorithm>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

class XboxMemory; 

class NV2ARenderer {
public:
    static constexpr uint32_t FB_WIDTH = 1280;
    static constexpr uint32_t FB_HEIGHT = 720;
    static constexpr uint32_t FB_SIZE = FB_WIDTH * FB_HEIGHT;
    static constexpr uint32_t TEXTURE_MEMORY = 128 * 1024 * 1024;
    static constexpr uint32_t MAX_VERTICES = 65536;
    static constexpr uint32_t MAX_COMMANDS = 16384;
   
    NV2ARenderer(XboxMemory* memory);
    NV2ARenderer();
    ~NV2ARenderer();
    
    void reset();
    void renderFrame();
    void processDMA();
    
    uint32_t readRegister(uint32_t addr);
    void writeRegister(uint32_t addr, uint32_t value);
    
    const uint32_t* getFramebuffer() const;
    uint32_t getFramebufferWidth() const { return FB_WIDTH; }
    uint32_t getFramebufferHeight() const { return FB_HEIGHT; }
    uint32_t getWidth() const;
    uint32_t getHeight() const; 
    
    void setDebugCallback(std::function<void(const std::string&)> callback);
    
    enum class GpuState {
        Ready,
        Processing,
        Error
    };
    
    GpuState getState() const;
    
    void setOutputResolution(uint32_t width, uint32_t height) {
        outputWidth = width;
        outputHeight = height;
        framebuffer.resize(width * height);
        updateScalingFactors();
    }
    
    void enableDepthTest(bool enable);
    void enableAlphaBlending(bool enable);
    void setClipRect(int left, int top, int right, int bottom);
    void enableVSync(bool enabled) { vsyncEnabled = enabled; }
    bool checkInterrupt() const { return false; /* TODO: Implement */ }

    uint32_t getOutputWidth() const { return outputWidth; }
    uint32_t getOutputHeight() const { return outputHeight; }

private:
    struct Vertex {
        float x, y, z;
        float u, v;
        uint32_t color;
        float fog;
    };
    
    struct TextureInfo {
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint32_t address;
        uint32_t pitch;
        uint32_t mipLevels;
        bool swizzled;
    };
    
    enum class PrimitiveType {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip,
        TriangleFan,
        Quads,
        QuadStrip,
        Polygon
    };
    
    std::vector<uint32_t> framebuffer;
    std::vector<uint8_t> textureMemory;
    std::vector<Vertex> vertexBuffer;
    std::vector<float> depthBuffer;
    
    std::array<uint32_t, 0x10000> registers;
    std::array<TextureInfo, 32> textureUnits;
    
    struct {
        uint32_t source;
        uint32_t dest;
        uint32_t size;
        bool active;
    } dmaState;
    
    struct {
        uint32_t pc;
        uint32_t put;
        uint32_t get;
        bool fifoEmpty;
    } cmdState;
    
    struct {
        int left, top, right, bottom;
    } clipRect;
    
    GpuState currentState;
    PrimitiveType currentPrimitive;
    uint32_t currentTexture;
    bool depthTestEnabled;
    bool alphaBlendEnabled;
    bool textureFilteringEnabled;
    bool textureSwizzlingEnabled;
    float anisotropicFiltering;
    uint32_t frameCounter;
    bool vsyncEnabled;
    
    uint32_t outputWidth = FB_WIDTH;
    uint32_t outputHeight = FB_HEIGHT;
    
    XboxMemory* memory;
    std::thread* renderThread;
    std::mutex renderMutex;
    std::condition_variable renderCond;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    
    std::function<void(const std::string&)> debugCallback;
    
    void clearFramebuffer(uint32_t color);
    void processCommandBuffer();
    void processVertices();
    void renderThreadFunc();
    
    void handlePrimitive(uint32_t command);
    void handleTextureUpload(uint32_t command);
    void handleVertexData(uint32_t command);
    void handleRegisterWrite(uint32_t reg, uint32_t value);
    void handleSpecialCommand(uint32_t command);
    
    void processPoints();
    void processLines();
    void processLineStrip();
    void processTriangles();
    void processTriangleStrip();
    void processTriangleFan();
    void processQuads();
    void processQuadStrip();
    void processPolygon();
    
    void drawPoint(const Vertex& v);
    void drawLine(const Vertex& v0, const Vertex& v1);
    void drawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);
    void drawQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);
    
    void drawTriangleNEON(const Vertex& v0, const Vertex& v1, const Vertex& v2);
    void drawLineNEON(const Vertex& v0, const Vertex& v1);
    
    uint32_t sampleTexture(float u, float v, uint32_t texUnit);
    uint32_t sampleTextureNearest(float x, float y, const TextureInfo& tex);
    uint32_t sampleTextureBilinear(float x, float y, const TextureInfo& tex);
    uint32_t blendColors(uint32_t color1, uint32_t color2);
    uint32_t blendPixels(uint32_t src, uint32_t dst);
    uint32_t bilinearInterpolate(uint32_t c00, uint32_t c01, uint32_t c10, uint32_t c11, float fx, float fy);
    
    bool depthTest(int x, int y, float depth);
    
    void logDebug(const std::string& message);
    void updateDMA();
    void checkFifoStatus();
    
    void setupDefaultState();
    void uploadTexture(uint32_t dest, const uint8_t* src, uint32_t size);
    void downloadTexture(uint8_t* dest, uint32_t src, uint32_t size);
    
    void swizzleTexture(uint8_t* dest, const uint8_t* src, uint32_t width, uint32_t height);
    void deswizzleTexture(uint8_t* dest, const uint8_t* src, uint32_t width, uint32_t height);
    
    void updateScalingFactors() {
    
    }
};