// SPDX-FileCopyrightText: xanite original Project 2025
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file vulkan_wrapper.h
 * @brief Vulkan wrapper for Xbox original emulation on mobile devices
 * 
 * Vulkan API for rendering Xbox original games on Android
 */

#ifndef VULKAN_WRAPPER_H
#define VULKAN_WRAPPER_H

#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <vector>
#include <string>
#include <optional>
#include <set>
#include <memory>
#include <mutex>

class VulkanWrapper {
public:
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct XboxTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string path;
    };

    enum class ErrorCode {
        SUCCESS,
        INSTANCE_CREATION_FAILED,
        DEVICE_NOT_FOUND,
        SURFACE_CREATION_FAILED,
        SWAP_CHAIN_CREATION_FAILED,
        OUT_OF_MEMORY,
        FILE_NOT_FOUND,
        INVALID_STATE,
        NV2A_EMULATION_FAILED
    };

    VulkanWrapper();
    ~VulkanWrapper();

    // Core Vulkan Functions
    ErrorCode initVulkan(ANativeWindow* window);
    void cleanup();
    ErrorCode drawFrame();
    void setAssetManager(AAssetManager* manager);

    // Xbox Specific Functions
    ErrorCode loadXboxTexture(const std::string& texturePath);
    ErrorCode renderXboxFrame();
    void setGameData(std::shared_ptr<XboxHW::Xbox> xbox);

    // Utility Functions
    std::string getLastError() const;
    bool isInitialized() const;

private:
    // Vulkan objects
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    // Queues
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Swap chain
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent = {0, 0};
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    // Xbox Game Data
    std::shared_ptr<XboxHW::Xbox> currentXbox;
    std::vector<XboxTexture> gameTextures;
    mutable std::mutex textureMutex;

    // NV2A GPU Emulation
    VkBuffer nv2aVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory nv2aVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer nv2aIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory nv2aIndexBufferMemory = VK_NULL_HANDLE;
    VkDescriptorSetLayout nv2aDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool nv2aDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet nv2aDescriptorSet = VK_NULL_HANDLE;

    // Android specific
    ANativeWindow* window = nullptr;
    AAssetManager* assetManager = nullptr;

    // Error handling
    std::string lastError;
    bool initialized = false;

    // Debug and extensions
    #ifdef NDEBUG
        static constexpr bool ENABLE_VALIDATION_LAYERS = false;
    #else
        static constexpr bool ENABLE_VALIDATION_LAYERS = true;
    #endif

    static const std::vector<const char*> VALIDATION_LAYERS;
    static const std::vector<const char*> DEVICE_EXTENSIONS;

    // Helper functions
    ErrorCode createInstance();
    ErrorCode createSurface();
    ErrorCode pickPhysicalDevice();
    ErrorCode createLogicalDevice();
    ErrorCode createSwapChain();
    ErrorCode recreateSwapChain();
    ErrorCode cleanupSwapChain();
    ErrorCode createImageViews();
    ErrorCode createRenderPass();
    ErrorCode createGraphicsPipeline();
    ErrorCode createFramebuffers();
    ErrorCode createCommandPool();
    ErrorCode createCommandBuffers();
    ErrorCode createSyncObjects();
    ErrorCode createTextureImage(const std::string& path, XboxTexture& texture);
    ErrorCode createTextureImageView(XboxTexture& texture);
    ErrorCode createTextureSampler(XboxTexture& texture);

    // Xbox Specific Helpers
    ErrorCode initNV2AEmulation();
    ErrorCode setupNV2AVertexBuffer();
    ErrorCode setupNV2AIndexBuffer();
    ErrorCode createNV2ADescriptorSetLayout();
    ErrorCode createNV2ADescriptorPool();
    ErrorCode createNV2ADescriptorSet();

    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    ErrorCode readFile(const std::string& filename, std::vector<char>& outBuffer);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    ErrorCode createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                         VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                         VkDeviceMemory& bufferMemory);
    ErrorCode copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    ErrorCode transitionImageLayout(VkImage image, VkFormat format, 
                                  VkImageLayout oldLayout, VkImageLayout newLayout);
    ErrorCode copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    void setError(const std::string& error);
};

#endif // VULKAN_WRAPPER_H