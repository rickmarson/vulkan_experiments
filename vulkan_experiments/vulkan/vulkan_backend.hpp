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
#include <functional>  


// Interfaces

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct RenderPass {
    std::string name;
    size_t key = 0;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    
    std::vector<VkFramebuffer> swap_chain_framebuffers;
};

struct GraphicsPipelineConfig {
    std::string name;
    // shaders
    std::vector<char> vert_code;
    std::vector<char> frag_code;
    // TODO: add tessellation and geometry
    // vertex buffer
    std::vector<VkVertexInputBindingDescription> vertex_buffer_binding_desc;
    std::vector<VkVertexInputAttributeDescription> vertex_buffer_attrib_desc;

    RenderPass render_pass;
};

struct GraphicsPipeline {
    std::string name;
    size_t key = 0;

    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline vk_graphics_pipeline = VK_NULL_HANDLE;
};

struct Buffer {
    std::string name;
    size_t key = 0;
   
    VkBufferUsageFlags type = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_buffer_memory = VK_NULL_HANDLE;
};

// VulkanBackend

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
    
    template<typename DataType>
    Buffer createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer);

    Buffer createIndexBuffer(const std::string& name, const std::vector<uint32_t>& src_buffer);

    template<typename DataType>
    void updateBuffer(Buffer dst_buffer, const std::vector<DataType>& src_buffer, bool is_staging_buffer = false);

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
    
    template<typename DataType>
    Buffer createBuffer(const std::string& name, 
                        const std::vector<DataType>& src_buffer, 
                        VkBufferUsageFlags buffer_usage, 
                        VkSharingMode sharing_mode,
                        bool host_visible,
                        bool persist);

    VkDeviceMemory allocateDeviceMemory(VkBuffer buffer, VkMemoryPropertyFlags properties);
    void copyBufferToGpuLocalMemory(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

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
    std::unordered_map<size_t, Buffer> gpu_local_buffers_;
    std::unordered_map<size_t, Buffer> host_visible_buffers_;
};

// inlines

template<typename DataType>
Buffer VulkanBackend::createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer) {
    Buffer staging_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true, false);
    updateBuffer<DataType>(staging_buffer, src_buffer, true);
    Buffer vertex_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, false, true);
    copyBufferToGpuLocalMemory(staging_buffer.vk_buffer, vertex_buffer.vk_buffer, sizeof(DataType) * src_buffer.size());
    vkDestroyBuffer(device_, staging_buffer.vk_buffer, nullptr);
    vkFreeMemory(device_, staging_buffer.vk_buffer_memory, nullptr);
    return vertex_buffer;
}

template<typename DataType>
Buffer VulkanBackend::createBuffer(const std::string& name, 
                                   const std::vector<DataType>& src_buffer, 
                                   VkBufferUsageFlags buffer_usage, 
                                   VkSharingMode sharing_mode,
                                   bool host_visible,
                                   bool persist) {

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(DataType) * src_buffer.size();
    buffer_info.usage = buffer_usage;
    buffer_info.sharingMode = sharing_mode;
    
    VkBuffer vk_buffer;
    if (vkCreateBuffer(device_, &buffer_info, nullptr, &vk_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        return Buffer{};
    }

    VkMemoryPropertyFlags mem_properties;
    if (host_visible) {
        mem_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        mem_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VkDeviceMemory memory = allocateDeviceMemory(vk_buffer, mem_properties);

    if (memory != VK_NULL_HANDLE) {
        vkBindBufferMemory(device_, vk_buffer, memory, 0);
    } else {
        vkDestroyBuffer(device_, vk_buffer, nullptr);
        return Buffer{};
    }

    Buffer buffer;
    buffer.name = name;
    buffer.key = std::hash<std::string>{}(name);
    buffer.type = buffer_usage;
    buffer.vk_buffer = vk_buffer;
    buffer.vk_buffer_memory = memory;

    if (persist) {
        host_visible ? host_visible_buffers_.insert({ buffer.key, buffer }) : gpu_local_buffers_.insert({ buffer.key, buffer });
    }

    return buffer;
}

template<typename DataType>
void VulkanBackend::updateBuffer(Buffer dst_buffer, const std::vector<DataType>& src_buffer, bool is_staging_buffer) {
    if (!is_staging_buffer && host_visible_buffers_.find(dst_buffer.key) == host_visible_buffers_.end()) {
        std::cerr << "Cannot updateBuffer. Requested buffer " << dst_buffer.name << "[" << dst_buffer.key << "] is not a host visible buffer" << std::endl;
        return;
    }

    void* data;
    size_t bytes = sizeof(DataType) * src_buffer.size();
    vkMapMemory(device_, dst_buffer.vk_buffer_memory, 0, bytes, 0, &data);
    memcpy(data, src_buffer.data(), bytes);
    vkUnmapMemory(device_, dst_buffer.vk_buffer_memory);
}