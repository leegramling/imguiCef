#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

class VulkanRenderer {
public:
    bool Initialize(GLFWwindow* window);
    void Cleanup();
    void BeginFrame();
    void EndFrame();
    
    VkCommandBuffer GetCommandBuffer() { return m_CommandBuffer; }
    VkInstance GetInstance() { return m_Instance; }
    VkDevice GetDevice() { return m_Device; }
    VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
    VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
    VkRenderPass GetRenderPass() { return m_RenderPass; }
    VkDescriptorPool GetDescriptorPool() { return m_DescriptorPool; }
    uint32_t GetQueueFamily() { return m_QueueFamily; }
    
    VkImage CreateTextureImage(uint32_t width, uint32_t height, const void* data);
    void UpdateTextureImage(VkImage image, uint32_t width, uint32_t height, const void* data);
    VkImageView CreateImageView(VkImage image, VkFormat format);
    VkSampler CreateTextureSampler();

private:
    GLFWwindow* m_Window = nullptr;
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    
    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;
    std::vector<VkFramebuffer> m_Framebuffers;
    
    uint32_t m_QueueFamily = 0;
    uint32_t m_ImageIndex = 0;
    
    VkSemaphore m_ImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_RenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_InFlightFence = VK_NULL_HANDLE;
    
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateDescriptorPool();
    bool CreateSyncObjects();
    
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};