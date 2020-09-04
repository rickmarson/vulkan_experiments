/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct RenderPass {
    std::string name;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    size_t key = 0;

    std::vector<VkFramebuffer> swap_chain_framebuffers;
};

struct GraphicsPipelineConfig {
    std::string name;
    std::vector<char> vert_code;
    std::vector<char> frag_code;
    RenderPass render_pass;
};

struct GraphicsPipeline {
    std::string name;
    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline vk_graphics_pipeline = VK_NULL_HANDLE;
    size_t key = 0;
};

class VulkanBackend {
public:
    bool createInstance(uint32_t required_extensions_count, const char** required_extensions, VkInstance& instance_out);

    void setWindowSurface(VkSurfaceKHR surface, VkExtent2D size) { 
        window_surface_ = surface; 
        window_swap_extent_ = size;
    }

    void resetWindowSwapExtent(VkExtent2D extent) { window_swap_extent_ = extent; }
    VkExtent2D getSwapChainExtent() const { return window_swap_extent_; }

    bool startUp();
    void shutDown();

    bool recreateSwapChain();
    
    RenderPass createRenderPass(const std::string& name);
    GraphicsPipeline createGraphicsPipeline(const GraphicsPipelineConfig& config);

    std::vector<VkCommandBuffer>& getCommandBuffers() { return command_buffers_; }

    VkResult submitCommands();

private:
    bool initVulkan();
    
    bool selectDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();

    void cleanupSwapChain();

    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    const uint32_t max_frames_in_flight_ = 2;
    size_t current_frame_ = 0;
    VkExtent2D window_swap_extent_ = { 0, 0 };
    SwapChainSupportDetails swap_chain_support_;

    VkInstance vk_instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR window_surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;  // logical device
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE; // display to window
    VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swap_chain_images_;
    VkFormat swap_chain_image_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swap_chain_extent_ = { 0,0 };
    std::vector<VkImageView> swap_chain_image_views_;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;  // one for every queue
    std::vector<VkCommandBuffer> command_buffers_;  // one for every image in the swap chain
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;

    std::unordered_map<size_t, RenderPass> render_passes_;
    std::unordered_map<size_t, GraphicsPipeline> pipelines_;
};