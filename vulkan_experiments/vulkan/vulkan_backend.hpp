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
class ShaderModule;
class Texture;

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
    std::shared_ptr<ShaderModule> vertex;
    std::shared_ptr<ShaderModule> fragment = nullptr;
    // TODO: add tessellation and geometry
    // vertex buffer
    VkVertexInputBindingDescription vertex_buffer_binding_desc;
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

struct UniformBuffer {
    std::string name;
    size_t key = 0;

    std::vector<Buffer> buffers;  // one per command buffer / swap chain image
    std::vector<VkDescriptorSet> descriptors; // one per command buffer / swap chain image
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
    
    std::shared_ptr<ShaderModule> createShaderModule(const std::string& name) const;
    std::shared_ptr<Texture> createTexture(const std::string& name);

    RenderPass createRenderPass(const std::string& name);
    GraphicsPipeline createGraphicsPipeline(const GraphicsPipelineConfig& config);
    
    template<typename DataType>
    Buffer createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer);

    Buffer createIndexBuffer(const std::string& name, const std::vector<uint32_t>& src_buffer);

    template<typename DataType>
    void updateBuffer(Buffer dst_buffer, const std::vector<DataType>& src_buffer);

    template<typename DataType>
    UniformBuffer createUniformBuffer(const std::string base_name, VkDescriptorSetLayout buffer_layout);

    std::vector<VkCommandBuffer>& getCommandBuffers() { return command_buffers_; }

    VkResult submitCommands();

private:
    friend class Texture;

    bool initVulkan();
    
    bool selectDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createDescriptorPool();
    bool createSyncObjects();

    void cleanupSwapChain();

    template<typename DataType>
    Buffer createBuffer(const std::string& name,
                        const std::vector<DataType>& src_buffer,
                        VkBufferUsageFlags buffer_usage,
                        VkSharingMode sharing_mode,
                        bool host_visible,
                        bool persist);

    VkDeviceMemory allocateDeviceMemory(VkMemoryRequirements mem_reqs, VkMemoryPropertyFlags properties);
    void copyBufferToGpuLocalMemory(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

    const uint32_t max_frames_in_flight_ = 2;
    const uint32_t max_descriptor_sets_ = 5; 
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
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;  // one for every image in the swap chain
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;

    std::unordered_map<size_t, RenderPass> render_passes_;
    std::unordered_map<size_t, GraphicsPipeline> pipelines_;
    std::unordered_map<size_t, Buffer> gpu_local_buffers_;
    std::unordered_map<size_t, Buffer> host_visible_buffers_;
    std::unordered_map<size_t, UniformBuffer> uniform_buffers_;
};

// inlines

template<typename DataType>
Buffer VulkanBackend::createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer) {
    Buffer staging_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true, false);
    updateBuffer<DataType>(staging_buffer, src_buffer);
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
        std::cerr << "Failed to create buffer!" << std::endl;
        return Buffer{};
    }

    VkMemoryPropertyFlags mem_properties;
    if (host_visible) {
        mem_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        mem_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, vk_buffer, &mem_reqs);

    VkDeviceMemory memory = allocateDeviceMemory(mem_reqs, mem_properties);

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
void VulkanBackend::updateBuffer(Buffer dst_buffer, const std::vector<DataType>& src_buffer) {
    // Note: Buffer must be host visible
    void* data;
    size_t bytes = sizeof(DataType) * src_buffer.size();
    vkMapMemory(device_, dst_buffer.vk_buffer_memory, 0, bytes, 0, &data);
    memcpy(data, src_buffer.data(), bytes);
    vkUnmapMemory(device_, dst_buffer.vk_buffer_memory);
}

template<typename DataType>
UniformBuffer VulkanBackend::createUniformBuffer(const std::string base_name, VkDescriptorSetLayout buffer_layout) {
    std::vector<Buffer> buffers;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(DataType);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkMemoryPropertyFlags mem_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        auto name = base_name + std::to_string(i);

        VkBuffer vk_buffer;
        if (vkCreateBuffer(device_, &buffer_info, nullptr, &vk_buffer) != VK_SUCCESS) {
            std::cerr << "Failed to create uniform buffer!" << std::endl;
            return UniformBuffer{};
        }

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device_, vk_buffer, &mem_reqs);

        VkDeviceMemory memory = allocateDeviceMemory(mem_reqs, mem_properties);

        if (memory != VK_NULL_HANDLE) {
            vkBindBufferMemory(device_, vk_buffer, memory, 0);
        }
        else {
            vkDestroyBuffer(device_, vk_buffer, nullptr);
            return UniformBuffer{};
        }

        Buffer buffer;
        buffer.name = name;
        buffer.key = std::hash<std::string>{}(name);
        buffer.type = buffer_info.usage;
        buffer.vk_buffer = vk_buffer;
        buffer.vk_buffer_memory = memory;

        buffers.push_back(buffer);
    }

    std::vector<VkDescriptorSetLayout> layouts(swap_chain_images_.size(), buffer_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images_.size());
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptor_sets(swap_chain_images_.size());
    if (vkAllocateDescriptorSets(device_, &alloc_info, descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets!" << std::endl;
        descriptor_sets.clear();

        for (auto& buffer : buffers) {
            vkDestroyBuffer(device_, buffer.vk_buffer, nullptr);
            vkFreeMemory(device_, buffer.vk_buffer_memory, nullptr);
        }
        buffers.clear();

        return UniformBuffer{};
    }

    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buffers[i].vk_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(DataType);

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buffer_info;
        descriptor_write.pImageInfo = nullptr; // Optional
        descriptor_write.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
    }

    UniformBuffer uniform_buffer;
    uniform_buffer.name = base_name; 
    uniform_buffer.key = std::hash<std::string>{}(base_name);
    uniform_buffer.buffers = std::move(buffers);
    uniform_buffer.descriptors = std::move(descriptor_sets);

    uniform_buffers_.insert({ uniform_buffer.key, uniform_buffer });
    return uniform_buffer;
}
