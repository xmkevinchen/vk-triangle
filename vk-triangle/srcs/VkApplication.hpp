//
//  VkApplication.hpp
//  vk-triangle
//
//  Created by Kai Chen on 10/22/22.
//

#ifndef VkApplication_hpp
#define VkApplication_hpp

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "VkBootstrap.h"

class VkApplication {
public:
    void run();
    
private:
    
    GLFWwindow *window = nullptr;
    vkb::Instance vkbInstance;
    VkSurfaceKHR vkSurface;
    vkb::Device vkbDevice;
    vkb::Swapchain vkbSwapchain;
    
    struct RenderData {
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
        
        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
        
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<VkSemaphore> availableSemaphores;
        std::vector<VkSemaphore> finishedSemaphores;
        std::vector<VkFence> inflightFences;
        std::vector<VkFence> imageInflight;
        
        size_t currentFrame = 0;
        
    } data;
    
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    
    void createDevice();
    void createSwapchain();
    void initQueues();
    void createRenderPass();
    void createGraphicsPipeline();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    
    void recreateSwapchain();
    void drawFrame();
};

#endif /* VkApplication_hpp */
