/*
* vulkan_backend.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include "common_definitions.hpp"

// Interfaces
class ShaderModule;
class Texture;
class Mesh;

struct RenderPassConfig {
    std::string name;
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    bool store_depth = false;
    bool enable_overlays = false;
};

struct GraphicsPipelineConfig {
    std::string name;
    // shaders
    std::shared_ptr<ShaderModule> vertex;
    std::shared_ptr<ShaderModule> fragment = nullptr;
    // TODO: add tessellation and geometry
    // vertex buffer desc
    VkVertexInputBindingDescription vertex_buffer_binding_desc;
    std::vector<VkVertexInputAttributeDescription> vertex_buffer_attrib_desc;

    // fixed function options
    bool cullBackFace = true;
    bool enableDepthTesting = true;
    bool enableStencilTest = false;
    bool enableTransparency = false;
    bool showWireframe = false;

    RenderPass render_pass;
    uint32_t subpass_number = 0;
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
    uint32_t getSwapChainSize() const { return swap_chain_images_.size(); }
    VkDescriptorPool getDescriptorPool() { return descriptor_pool_; }
    VkDevice getDevice() { return device_; }
    VkSampleCountFlagBits getMaxMSAASamples() const { return max_msaa_samples_; }

    bool startUp();
    void shutDown();
    void waitDeviceIdle() const;

    bool recreateSwapChain();
    
    std::shared_ptr<ShaderModule> createShaderModule(const std::string& name) const;
    std::shared_ptr<Texture> createTexture(const std::string& name);
    std::shared_ptr<Mesh> createMesh(const std::string& name);

    std::vector<VkCommandBuffer> createPrimaryCommandBuffers(uint32_t count) const; // the caller is responsible for managing these
    std::vector<VkCommandBuffer> createSecondaryCommandBuffers(uint32_t count) const; // the caller is responsible for managing these
    void resetCommandBuffers(std::vector<VkCommandBuffer>& cmd_buffers) const;
    void freeCommandBuffers(std::vector<VkCommandBuffer>& cmd_buffers) const;

    RenderPass createRenderPass(const RenderPassConfig& config);
    GraphicsPipeline createGraphicsPipeline(const GraphicsPipelineConfig& config);
    bool createDescriptorPool(uint32_t buffer_count, uint32_t sampler_count, uint32_t max_sets = 0);

    template<typename DataType>
    Buffer createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer, bool static_buffer = true);

    template<typename DataType>
    Buffer createIndexBuffer(const std::string& name, const std::vector<DataType>& src_buffer, bool static_buffer = true);

    template<typename DataType>
    void updateBuffer(Buffer& dst_buffer, const std::vector<DataType>& src_buffer);

    template<typename DataType>
    UniformBuffer createUniformBuffer(const std::string base_name);
    
    void updateDescriptorSets(const UniformBuffer& buffer, std::vector<VkDescriptorSet>& descriptor_sets, uint32_t binding);

    void destroyBuffer(Buffer& buffer);
    void destroyRenderPass(RenderPass& render_pass);
    void destroyGraphicsPipeline(GraphicsPipeline& graphics_pipeline);
    void destroyUniformBuffer(UniformBuffer& uniform_buffer);
    
    VkResult startNextFrame(uint32_t& next_swapchain_image, bool window_resized);
    VkResult submitCommands(uint32_t swapchain_image, const std::vector<VkCommandBuffer>& command_buffers);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer command_buffer);

private:
    friend class Texture;

    bool initVulkan();
    
    bool selectDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createCommandPool();
    bool createSyncObjects();

    void cleanupSwapChain();

    template<typename DataType>
    Buffer createBuffer(const std::string& name,
                        const std::vector<DataType>& src_buffer,
                        VkBufferUsageFlags buffer_usage,
                        VkSharingMode sharing_mode,
                        bool host_visible);

    VkDeviceMemory allocateDeviceMemory(VkMemoryRequirements mem_reqs, VkMemoryPropertyFlags properties);
    void copyBufferToGpuLocalMemory(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels = 1);

    VkPhysicalDevice getPhysicalDevice() const { return physical_device_; }

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
    VkSampleCountFlagBits max_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;  // one for every queue
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;
};

// inlines

template<typename DataType>
Buffer VulkanBackend::createVertexBuffer(const std::string& name, const std::vector<DataType>& src_buffer, bool static_buffer) {
    if (static_buffer) {
        Buffer staging_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true);
        updateBuffer<DataType>(staging_buffer, src_buffer);
        Buffer vertex_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, false);
        copyBufferToGpuLocalMemory(staging_buffer.vk_buffer, vertex_buffer.vk_buffer, sizeof(DataType) * src_buffer.size());
        destroyBuffer(staging_buffer);
        return vertex_buffer;
    } else {
        Buffer vertex_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, true);
        updateBuffer<DataType>(vertex_buffer, src_buffer);
        return vertex_buffer;
    }
}

template<typename DataType>
Buffer VulkanBackend::createIndexBuffer(const std::string& name, const std::vector<DataType>& src_buffer, bool static_buffer) {
    if (static_buffer) {
        Buffer staging_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, true);
        updateBuffer<DataType>(staging_buffer, src_buffer);
        Buffer index_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, false);
        copyBufferToGpuLocalMemory(staging_buffer.vk_buffer, index_buffer.vk_buffer, sizeof(DataType) * src_buffer.size());
        destroyBuffer(staging_buffer);
        return index_buffer;
    } else {
        Buffer index_buffer = createBuffer<DataType>(name, src_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, true);
        updateBuffer<DataType>(index_buffer, src_buffer);
        return index_buffer;
    }
}

template<typename DataType>
Buffer VulkanBackend::createBuffer(const std::string& name, 
                                   const std::vector<DataType>& src_buffer, 
                                   VkBufferUsageFlags buffer_usage, 
                                   VkSharingMode sharing_mode,
                                   bool host_visible) {

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
    buffer.host_visible = host_visible;
    buffer.type = buffer_usage;
    buffer.vk_buffer = vk_buffer;
    buffer.vk_buffer_memory = memory;

    return buffer;
}

template<typename DataType>
void VulkanBackend::updateBuffer(Buffer& dst_buffer, const std::vector<DataType>& src_buffer) {
    // Note: Buffer must be host visible
    void* data;
    size_t bytes = sizeof(DataType) * src_buffer.size();
    vkMapMemory(device_, dst_buffer.vk_buffer_memory, 0, bytes, 0, &data);
    memcpy(data, src_buffer.data(), bytes);
    vkUnmapMemory(device_, dst_buffer.vk_buffer_memory);
}

template<typename DataType>
UniformBuffer VulkanBackend::createUniformBuffer(const std::string base_name) {
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
        buffer.type = buffer_info.usage;
        buffer.vk_buffer = vk_buffer;
        buffer.vk_buffer_memory = memory;

        buffers.push_back(buffer);
    }

    UniformBuffer uniform_buffer;
    uniform_buffer.name = base_name;
    uniform_buffer.buffer_size = sizeof(DataType);
    uniform_buffer.buffers = std::move(buffers);

    return uniform_buffer;
}