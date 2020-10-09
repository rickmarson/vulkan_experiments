/*
* vulkan_backend.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_backend.hpp"
#include "shader_module.hpp"
#include "texture.hpp"
#include "mesh.hpp"

#include <optional>
#include <algorithm>

// Additional Helper Functions

namespace {

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> additional_required_ext = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool isValid() {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    void printSupportedExtensions() {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        std::cout << "available extensions:" << std::endl;

        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << std::endl;
        }
    }

    bool checkValidationLayerSupport() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (auto layer_name : validation_layers) {
            bool layer_found = false;

            for (const auto& layerProperties : available_layers) {
                if (std::string(layer_name) == std::string(layerProperties.layerName)) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    bool checkAdditionalRequiredExtensionsSupport(VkPhysicalDevice device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(additional_required_ext.begin(), additional_required_ext.end());

        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR window_surface) {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families) {
            // to keep things simple, grab a queue that can support both graphics and compute
            // note: transfer is implictly supported by both graphics and compute queues
            if (queue_family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                indices.graphics_family = i;
            }
            // we also need a queue that can support drawing to a window (can be the graphics queue or not)
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window_surface, &presentSupport);
            if (presentSupport) {
                indices.present_family = i;
            }

            if (indices.isValid()) {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR window_surface) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, window_surface, &details.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, nullptr);

        if (format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, nullptr);

        if (present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for (const auto& format : available_formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return available_formats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
        for (const auto& mode : available_present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;  // this is always guaranteed to be available
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actual_extent) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;  // this will match the window size in this case
        }
        else {
            actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
            actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

            return actual_extent;
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkPhysicalDevice physical_device, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        return 0;
    }

    VkSampleCountFlagBits getMaxSupportedSampleCount(VkPhysicalDevice physical_device) {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physical_device, &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
}
}

// VulkanBackend

bool VulkanBackend::createInstance(uint32_t required_extensions_count, const char** required_extensions, VkInstance& instance_out) {
    printSupportedExtensions();

    if (enable_validation_layers && !checkValidationLayerSupport()) {
        std::cerr << "validation layers requested, but not available!" << std::endl;
        return false;
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VulkanApp";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    std::cout << "Required Extensions [ " << required_extensions_count << "]:" << std::endl;
    for (uint32_t i = 0; i < required_extensions_count; i++) {
        std::cout << "\t" << required_extensions[i] << std::endl;
    }

    create_info.enabledExtensionCount = required_extensions_count;
    create_info.ppEnabledExtensionNames = required_extensions;

    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&create_info, nullptr, &vk_instance_) != VK_SUCCESS) {
        std::cout << "failed to create instance!" << std::endl;
        return false;
    }

    instance_out = vk_instance_;

    return true;
}

bool VulkanBackend::startUp() {
    return initVulkan();
}

void VulkanBackend::shutDown() {
    cleanupSwapChain();
    
    for (uint32_t i = 0; i < max_frames_in_flight_; i++) {
        vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
        vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
        vkDestroyFence(device_, in_flight_fences_[i], nullptr);
    }

    vkDestroyCommandPool(device_, command_pool_, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(vk_instance_, window_surface_, nullptr);
    vkDestroyInstance(vk_instance_, nullptr);

    swap_chain_images_.clear();
}

void VulkanBackend::waitDeviceIdle() const {
    vkDeviceWaitIdle(device_);
}

std::shared_ptr<ShaderModule> VulkanBackend::createShaderModule(const std::string& name) const {
    return ShaderModule::createShaderModule(name, device_);
}

std::shared_ptr<Texture> VulkanBackend::createTexture(const std::string& name) {
    return Texture::createTexture(name, device_, this);
}

std::shared_ptr<Mesh> VulkanBackend::createMesh(const std::string& name) {
    return Mesh::createMesh(name, this);
}

std::vector<VkCommandBuffer> VulkanBackend::createPrimaryCommandBuffers(uint32_t count) const {
    std::vector<VkCommandBuffer> cmd_buffers(count);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device_, &alloc_info, cmd_buffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!" << std::endl;
        cmd_buffers.clear();
        return cmd_buffers;
    }

    return cmd_buffers;
}

std::vector<VkCommandBuffer> VulkanBackend::createSecondaryCommandBuffers(uint32_t count) const {
    std::vector<VkCommandBuffer> secondary_cmd_buffers(count);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    alloc_info.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device_, &alloc_info, secondary_cmd_buffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!" << std::endl;
        secondary_cmd_buffers.clear();
        return secondary_cmd_buffers;
    }

    return secondary_cmd_buffers;
}

void VulkanBackend::resetCommandBuffers(std::vector<VkCommandBuffer>& cmd_buffers) const {
    for (auto& buffer : cmd_buffers) {
        vkResetCommandBuffer(buffer, 0);
    }
}

void VulkanBackend::freeCommandBuffers(std::vector<VkCommandBuffer>& cmd_buffers) const {
    vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(cmd_buffers.size()), cmd_buffers.data());
    cmd_buffers.clear();
}

RenderPass VulkanBackend::createRenderPass(const RenderPassConfig& config) {
    // create multisampled colour attachment
    auto extent = getSwapChainExtent();
   
    auto colour_texture = Texture::createTexture(config.name + "_colour_attachment", device_, this);
    colour_texture->createColourAttachment(extent.width, extent.height, swap_chain_image_format_, config.msaa_samples);

    // create depth attachment
    auto depth_texture = Texture::createTexture(config.name + "_depth_attachment", device_, this);
    depth_texture->createDepthStencilAttachment(extent.width, extent.height, config.msaa_samples);

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swap_chain_image_format_;
    color_attachment.samples = config.msaa_samples;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_texture->getFormat();
    depth_attachment.samples = config.msaa_samples;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = config.store_depth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription color_attachment_resolve{};
    color_attachment_resolve.format = swap_chain_image_format_;
    color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_resolve_ref{};
    color_attachment_resolve_ref.attachment = 2;
    color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> subpass_dependencies;
    auto subpass_count = config.enable_overlays ? 2 : 1;
    subpasses.resize(subpass_count);
    subpass_dependencies.resize(subpass_count);
    
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &color_attachment_ref;
    subpasses[0].pDepthStencilAttachment = &depth_attachment_ref;

    if (!config.enable_overlays) {
        subpasses[0].pResolveAttachments = &color_attachment_resolve_ref;
    }
    else {
        subpasses[0].pResolveAttachments = nullptr;
    }

    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = config.enable_overlays ? 1 : 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[0].srcAccessMask = 0;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (config.enable_overlays) {
        subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[1].colorAttachmentCount = 1;
        subpasses[1].pColorAttachments = &color_attachment_ref;
        subpasses[1].pDepthStencilAttachment = nullptr;
        subpasses[1].pResolveAttachments = &color_attachment_resolve_ref;

        subpass_dependencies[1].srcSubpass = 0;
        subpass_dependencies[1].dstSubpass = 1;
        subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    std::array<VkAttachmentDescription, 3> attachments = { color_attachment, depth_attachment, color_attachment_resolve };

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());;
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
    render_pass_info.pSubpasses = subpasses.data();
    render_pass_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
    render_pass_info.pDependencies = subpass_dependencies.data();

    VkRenderPass vk_render_pass;
    if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &vk_render_pass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass!" << std::endl;
        return RenderPass{};
    }

    RenderPass render_pass;
    render_pass.name = config.name;
    render_pass.msaa_samples = config.msaa_samples;
    render_pass.vk_render_pass = vk_render_pass;
    render_pass.colour_attachment = std::move(colour_texture);
    render_pass.depth_attachment = std::move(depth_texture);

    // create the swapchain framebuffers for this render pass
    render_pass.swap_chain_framebuffers.resize(swap_chain_image_views_.size());

    for (size_t i = 0; i < swap_chain_image_views_.size(); i++) {
        std::array<VkImageView, 3> framebuffer_attachments = {
            render_pass.colour_attachment->getImageView(),
            render_pass.depth_attachment->getImageView(),
            swap_chain_image_views_[i]
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk_render_pass;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(framebuffer_attachments.size());
        framebuffer_info.pAttachments = framebuffer_attachments.data();
        framebuffer_info.width = swap_chain_extent_.width;
        framebuffer_info.height = swap_chain_extent_.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &render_pass.swap_chain_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer!" << std::endl;
            // TODO cleanup
            return RenderPass{};
        }
    }

    return render_pass;
}

GraphicsPipeline VulkanBackend::createGraphicsPipeline(const GraphicsPipelineConfig& config) {
    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = config.vertex->getShader();
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = config.fragment->getShader();
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    // assemble layout information from all shaders
    DescriptorSetMetadata pipeline_descriptor_metadata{};
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> layout_bindings_by_set;

    const auto& vertex_layouts = config.vertex->getDescriptorSetLayouts();
    for (const auto& layout : vertex_layouts) {
        if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
            layout_bindings_by_set[layout.id] = layout.layout_bindings;
        }
        else {
            auto& bindings_array = layout_bindings_by_set[layout.id];
            bindings_array.insert(bindings_array.begin(), layout.layout_bindings.begin(), layout.layout_bindings.end());
        }
    }
    const auto& vertex_descriptor_metadata = config.vertex->getDescriptorsMetadata();
    for (const auto& meta : vertex_descriptor_metadata.set_bindings) {
        pipeline_descriptor_metadata.set_bindings[meta.first] = meta.second;
    }

    const auto& fragment_layouts = config.fragment->getDescriptorSetLayouts();
    for (const auto& layout : fragment_layouts) {
        if (layout_bindings_by_set.find(layout.id) == layout_bindings_by_set.end()) {
            layout_bindings_by_set[layout.id] = layout.layout_bindings;
        }
        else {
            auto& bindings_array = layout_bindings_by_set[layout.id];
            bindings_array.insert(bindings_array.end(), layout.layout_bindings.begin(), layout.layout_bindings.end());
        }
    }
    const auto& fragment_descriptor_metadata = config.fragment->getDescriptorsMetadata();
    for (const auto& meta : fragment_descriptor_metadata.set_bindings) {
        auto& bindings_map = pipeline_descriptor_metadata.set_bindings[meta.first];
        for (const auto& src_binding : meta.second) {
            bindings_map.insert(src_binding);
        }
    }

    // create descriptor layouts for all sets of binding points in the pipeline
    std::map<uint32_t, VkDescriptorSetLayout> descriptors_set_layouts;
    std::map<uint32_t, std::vector< VkDescriptorSet>> descriptor_sets;
    for (auto& set : layout_bindings_by_set) {
        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<uint32_t>(set.second.size());
        layout_create_info.pBindings = set.second.data();

        VkDescriptorSetLayout descriptor_set_layout;
        if (vkCreateDescriptorSetLayout(device_, &layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            std::cerr << "Failed to create descriptor set layout!" << std::endl;
            return GraphicsPipeline{};
        }

        descriptors_set_layouts[set.first] = descriptor_set_layout;
    }

    // auxiliary array to make sure the layouts are ordered and contiguous in memory
    std::vector<VkDescriptorSetLayout> descriptors_set_layouts_aux;
    for (auto& layout : descriptors_set_layouts) {
        descriptors_set_layouts_aux.push_back(layout.second);
    }

    // assemble push constants  
    std::vector<VkPushConstantRange> push_constants_array;
    PushConstantsMap push_constants_map;
    auto compare = [](const PushConstantBlock& a, const PushConstantBlock& b) { return a.name < b.name; };
    std::set<PushConstantBlock, decltype(compare)> push_constants_temp(compare);

    const auto& vertex_push_constants = config.vertex->getPushConstants();
    const auto& fragment_push_constants = config.fragment->getPushConstants();

    for (const auto& pc : vertex_push_constants) {
        push_constants_temp.insert(pc);
    }

    for (const auto& pc : fragment_push_constants) {
        auto dst_iter = push_constants_temp.find(pc);
        if ( dst_iter == push_constants_temp.end()) {
            push_constants_temp.insert(pc);
        } else {
            auto new_block = *(dst_iter);
            new_block.push_constant_range.stageFlags |= pc.push_constant_range.stageFlags;
            push_constants_temp.erase(dst_iter);
            push_constants_temp.insert(new_block);
        }
    }
    
    for (auto& pc : push_constants_temp) {
        push_constants_array.push_back(pc.push_constant_range);
        push_constants_map.insert({pc.name, pc.push_constant_range});
    }
    push_constants_temp.clear();

    // fixed functionality configuration

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &config.vertex_buffer_binding_desc;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertex_buffer_attrib_desc.size());
    vertex_input_info.pVertexAttributeDescriptions = config.vertex_buffer_attrib_desc.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swap_chain_extent_.width;
    viewport.height = (float)swap_chain_extent_.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swap_chain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.showWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = config.cullBackFace ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.rasterizationSamples = config.render_pass.msaa_samples;
    multisampling.minSampleShading = 0.2f;
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = config.enableTransparency ? VK_TRUE : VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; 
    color_blend_attachment.srcAlphaBlendFactor = config.enableTransparency ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; 
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; 

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f; // Optional
    color_blending.blendConstants[1] = 0.0f; // Optional
    color_blending.blendConstants[2] = 0.0f; // Optional
    color_blending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = config.enableDepthTesting ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = config.enableDepthTesting ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = config.enableStencilTest ? VK_TRUE : VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptors_set_layouts_aux.size());
    pipeline_layout_info.pSetLayouts = descriptors_set_layouts_aux.data();
    pipeline_layout_info.pushConstantRangeCount = push_constants_array.size();
    pipeline_layout_info.pPushConstantRanges = push_constants_array.size() > 0 ? push_constants_array.data() : nullptr;

    VkPipelineLayout pipeline_layout;
    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!" << std::endl;
        return GraphicsPipeline{};
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr; // Optional
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = config.render_pass.vk_render_pass;
    pipeline_info.subpass = config.subpass_number;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional
    
    VkPipeline vk_pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &vk_pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline!" << std::endl;
        return GraphicsPipeline{};
    }

    auto key = std::hash<std::string>{}(config.name);
    GraphicsPipeline graphics_pipeline{ 
        config.name, 
        pipeline_layout, 
        vk_pipeline,
        std::move(descriptors_set_layouts),
        std::move(pipeline_descriptor_metadata),
        std::move(push_constants_map)
    };

    return graphics_pipeline;
}

VkResult VulkanBackend::startNextFrame(uint32_t& swapchain_image, bool window_resized) {
    if (window_resized) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device_, swap_chain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &swapchain_image);

    auto needs_rebuilding = (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);
    auto error = (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR);

    if (error) {
        std::cerr << "Failed to acquire swap chain image!" << std::endl;
    }
    if (needs_rebuilding) {
        std::cerr << "Swapchain is out of date. Rebuilding..." << std::endl;
    }
    if (error || needs_rebuilding) {
        // clear the image semaphore as we won't be submitting the queue that waits for it this frame
        vkDestroySemaphore(device_, image_available_semaphores_[current_frame_], nullptr);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[current_frame_]) != VK_SUCCESS) {
            std::cerr << "Failed to reset the semaphore for swapchain image " << current_frame_ << std::endl;
        }   
    }

    return result;
}

VkResult VulkanBackend::submitCommands(uint32_t swapchain_image, const std::vector<VkCommandBuffer>& command_buffers) {
    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (images_in_flight_[swapchain_image] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &images_in_flight_[swapchain_image], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    images_in_flight_[swapchain_image] = in_flight_fences_[current_frame_];

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores_[current_frame_] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();

    VkSemaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer!" << std::endl;
        return result;
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { swap_chain_ };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &swapchain_image;
    present_info.pResults = nullptr; // Optional

    result = vkQueuePresentKHR(present_queue_, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return result;        
    }
    else if (result != VK_SUCCESS) {
        std::cerr << "Failed to present swap chain image!" << std::endl;
        return result;
    }

    current_frame_ = (current_frame_ + 1) % max_frames_in_flight_;

    return result;
}

VkCommandBuffer VulkanBackend::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanBackend::endSingleTimeCommands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);

    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

bool VulkanBackend::initVulkan() {
    if (!selectDevice()) {
        return false;
    }
    if (!createLogicalDevice()) {
        return false;
    }
    if (!createSwapChain()) {
        return false;
    }
    if (!createImageViews()) {
        return false;
    }
    if (!createCommandPool()) {
        return false;
    }
    if (!createSyncObjects()) {
        return false;
    }
    return true;
}

bool VulkanBackend::selectDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance_, &device_count, nullptr);
    if (device_count == 0) {
        std::cerr << "failed to find GPUs with Vulkan support!" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(vk_instance_, &device_count, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physical_device_ = device;  // fairly confident there's only one discrete GPU in our use case
            max_msaa_samples_ = getMaxSupportedSampleCount(device);
            break;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        std::cerr << "failed to find a suitable GPU!" << std::endl;
        return false;
    }

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device_, &device_properties);
    std::cout << "Selected Device: " << device_properties.deviceName << std::endl;

    return true;
}

bool VulkanBackend::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    QueueFamilyIndices indices = findQueueFamilies(device, window_surface_);

    bool extensions_supported = checkAdditionalRequiredExtensionsSupport(device);

    bool swap_chain_adequate = false;
    if (extensions_supported) {
        SwapChainSupportDetails swap_chain_support = querySwapChainSupport(device, window_surface_);
        swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
    }

    return device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        device_features.geometryShader && indices.isValid() && extensions_supported && swap_chain_adequate
        && device_features.samplerAnisotropy;
}

bool VulkanBackend::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physical_device_, window_surface_);
    float queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphics_family.value(), indices.present_family.value() };

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = indices.graphics_family.value();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(additional_required_ext.size());
    create_info.ppEnabledExtensionNames = additional_required_ext.data();

    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size()); // should be ignored by recent implementations
        create_info.ppEnabledLayerNames = validation_layers.data();  // should be ignored by recent implementations
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
        std::cerr << "failed to create logical device!" << std::endl;
        return false;
    }

    vkGetDeviceQueue(device_, indices.graphics_family.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, indices.present_family.value(), 0, &present_queue_);

    return true;
}

bool VulkanBackend::createSwapChain() {
    swap_chain_support_ = querySwapChainSupport(physical_device_, window_surface_);

    swap_chain_extent_ = chooseSwapExtent(swap_chain_support_.capabilities, window_swap_extent_);

    VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(swap_chain_support_.formats);
    VkPresentModeKHR present_mode = chooseSwapPresentMode(swap_chain_support_.present_modes);

    uint32_t image_count = swap_chain_support_.capabilities.minImageCount + 1;

    if (swap_chain_support_.capabilities.maxImageCount > 0 && image_count > swap_chain_support_.capabilities.maxImageCount) {
        image_count = swap_chain_support_.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = window_surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = window_swap_extent_;
    create_info.imageArrayLayers = 1;  // always 1 except for stereoscopic 3D
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physical_device_, window_surface_);
    uint32_t queueFamilyIndices[] = { indices.graphics_family.value(), indices.present_family.value() };

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }

    create_info.preTransform = swap_chain_support_.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // for blending with other windows
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;  // resizing the window invalidates the swap chain. pass the handle to the old one here when re-creating it

    if (vkCreateSwapchainKHR(device_, &create_info, nullptr, &swap_chain_) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain!" << std::endl;
        return false;
    }

    vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, nullptr);
    swap_chain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, swap_chain_images_.data());
    swap_chain_image_format_ = surface_format.format;

    return true;
}

bool VulkanBackend::createImageViews() {
    swap_chain_image_views_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        swap_chain_image_views_[i] = createImageView(swap_chain_images_[i], swap_chain_image_format_, VK_IMAGE_ASPECT_COLOR_BIT);
        if (swap_chain_image_views_[i] == VK_NULL_HANDLE) {
            return false;
        }
    }

    return true;
}

bool VulkanBackend::createCommandPool() {
    QueueFamilyIndices queue_family_indices = findQueueFamilies(physical_device_, window_surface_);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanBackend::createDescriptorPool(uint32_t buffer_count, uint32_t sampler_count, uint32_t max_sets) {
    if (max_sets == 0) {
        max_sets = buffer_count * sampler_count * 2;
    }

    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = static_cast<uint32_t>(swap_chain_images_.size()) * buffer_count;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = static_cast<uint32_t>(swap_chain_images_.size()) * sampler_count;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(swap_chain_images_.size())* max_sets;

    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanBackend::createSyncObjects() {
    image_available_semaphores_.resize(max_frames_in_flight_);
    render_finished_semaphores_.resize(max_frames_in_flight_);
    in_flight_fences_.resize(max_frames_in_flight_);
    images_in_flight_.resize(swap_chain_images_.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // avoids an infinite wait on the first wait on this fence if it has never signalled before

    for (uint32_t i = 0; i < max_frames_in_flight_; i++) {
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {

            std::cerr << "Failed to create sync objects for frame " << i << "!" << std::endl;
            return false;
        }
    }

    return true;
}

void VulkanBackend::cleanupSwapChain() {

    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);

    for (auto image_view : swap_chain_image_views_) {
        vkDestroyImageView(device_, image_view, nullptr);
    }

    vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
}

bool VulkanBackend::recreateSwapChain() {
    cleanupSwapChain();

    if (!createSwapChain()) {
        return false;
    }
    if (!createImageViews()) {
        return false;
    }
    
    return true;
}

VkDeviceMemory VulkanBackend::allocateDeviceMemory(VkMemoryRequirements mem_reqs, VkMemoryPropertyFlags properties) {
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, physical_device_, properties);

    VkDeviceMemory buffer_memory;
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory!" << std::endl;
        return VK_NULL_HANDLE;
    }

    return buffer_memory;
}

void VulkanBackend::copyBufferToGpuLocalMemory(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
    VkCommandBuffer command_buffer = beginSingleTimeCommands();
   
    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0; // Optional
    copy_region.dstOffset = 0; // Optional
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    endSingleTimeCommands(command_buffer);
}

void VulkanBackend::updateDescriptorSets(const UniformBuffer& buffer, std::vector<VkDescriptorSet>& descriptor_sets, uint32_t binding) {
    for (size_t i = 0; i < descriptor_sets.size(); i++) {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buffer.buffers[i].vk_buffer;
        buffer_info.offset = 0;
        buffer_info.range = buffer.buffer_size;

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = binding;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buffer_info;
        descriptor_write.pImageInfo = nullptr; // Optional
        descriptor_write.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
    }
}

void VulkanBackend::destroyBuffer(Buffer& buffer) {
    vkDestroyBuffer(device_, buffer.vk_buffer, nullptr);
    vkFreeMemory(device_, buffer.vk_buffer_memory, nullptr);
    buffer = Buffer{};
}

void VulkanBackend::destroyRenderPass(RenderPass& render_pass) {
    for (auto& framebuffer : render_pass.swap_chain_framebuffers) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    render_pass.swap_chain_framebuffers.clear();
    render_pass.colour_attachment.reset();
    render_pass.depth_attachment.reset();

    vkDestroyRenderPass(device_, render_pass.vk_render_pass, nullptr);
    
    render_pass = RenderPass{};
}

void VulkanBackend::destroyGraphicsPipeline(GraphicsPipeline& graphics_pipeline) {
    for (auto& descr_set_layout : graphics_pipeline.vk_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(device_, descr_set_layout.second, nullptr);
    }
    
    vkDestroyPipeline(device_, graphics_pipeline.vk_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(device_, graphics_pipeline.vk_pipeline_layout, nullptr);
    graphics_pipeline.vk_descriptor_set_layouts.clear();
    graphics_pipeline = GraphicsPipeline{};
}

void VulkanBackend::destroyUniformBuffer(UniformBuffer& uniform_buffer) {
    for (auto& buffer : uniform_buffer.buffers) {
        vkDestroyBuffer(device_, buffer.vk_buffer, nullptr);
        vkFreeMemory(device_, buffer.vk_buffer_memory, nullptr);
    }
    uniform_buffer.buffers.clear();
    uniform_buffer.buffer_size = 0;
    uniform_buffer.name = "";
}

VkImageView VulkanBackend::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(device_, &view_info, nullptr, &image_view) != VK_SUCCESS) {
        std::cerr << "Failed to create texture image view!" << std::endl;
        return VK_NULL_HANDLE;
    }

    return image_view;
}