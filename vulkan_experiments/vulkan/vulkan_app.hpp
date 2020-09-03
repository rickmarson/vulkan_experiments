/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class VulkanApp {
public:
    bool setup();
    bool run();

    void onWindowResized() { window_resized_ = true; }

protected:
    // boilerplate creation and cleanup
    void createWindow();
    bool initVulkan();
    bool createInstance();
    bool selectDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();

    void cleanupSwapChain();
    bool recreateSwapChain();

    virtual bool createSwapChainFramebuffers() = 0;
    virtual bool createRenderPass() = 0;
    virtual bool createGraphicsPipeline() = 0;

    void cleanup();

    // actual rendering code
    void mainLoop();
    void drawFrame();

    virtual bool setupScene() = 0;
    virtual void updateScene() = 0;

    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    // window
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;
    bool window_resized_ = false;

    size_t current_frame_ = 0;

    // vulkan API
    const uint32_t max_frames_in_flight_ = 2;

    VkInstance vk_instance_;
    VkSurfaceKHR window_surface_;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_;  // logical device
    VkQueue graphics_queue_;
    VkQueue present_queue_; // display to window
    VkSwapchainKHR swap_chain_;
    std::vector<VkImage> swap_chain_images_;
    VkFormat swap_chain_image_format_;
    VkExtent2D swap_chain_extent_;
    std::vector<VkImageView> swap_chain_image_views_;
    std::vector<VkFramebuffer> swap_chain_framebuffers_;
    VkCommandPool command_pool_;  // one for every queue
    std::vector<VkCommandBuffer> command_buffers_;  // one for every image in the swap chain
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;

    VkRenderPass render_pass_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline graphics_pipeline_;
};
