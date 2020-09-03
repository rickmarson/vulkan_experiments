/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <set>
#include <optional>
#include <algorithm>
#include <iostream>

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

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
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

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;  // this will match the window size in this case
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actual_extent = { width, height };

            actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
            actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

            return actual_extent;
        }
    }

    void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
        app->onWindowResized();
    }
}

// VulkanApp

bool VulkanApp::setup() {
    createWindow();
    if (!initVulkan()) {
        return false;
    }
    return true;
}

bool VulkanApp::run() {
    if (!setupScene()) {
        return false;
    }
    mainLoop();
}

bool VulkanApp::initVulkan() {
    if (!createInstance()) {
        return false;
    }
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
    if (!createRenderPass()) {
        return false;
    }
    if (!createGraphicsPipeline()) {
        return false;
    }
    if (!createSwapChainFramebuffers()) {
        return false;
    }
    if (!createCommandPool()) {
        return false;
    }
    if (!createCommandBuffers()) {
        return false;
    }
    if (!createSyncObjects()) {
        return false;
    }
    return true;
}

void VulkanApp::createWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

bool VulkanApp::createInstance() {
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

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
   
    std::cout << "GLFW Required Extensions [ " << glfw_extension_count << "]:" << std::endl;
    for (uint32_t i = 0; i < glfw_extension_count; i++) {
        std::cout << "\t" << glfw_extensions[i] << std::endl;
    }

    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&create_info, nullptr, &vk_instance_) != VK_SUCCESS) {
        std::cout << "failed to create instance!" << std::endl;
    }

    // setup the window surface 
    if (glfwCreateWindowSurface(vk_instance_, window_, nullptr, &window_surface_) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!" << std::endl;
    }

    return true;
}

bool VulkanApp::selectDevice() {
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

bool VulkanApp::isDeviceSuitable(VkPhysicalDevice device) {
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
        device_features.geometryShader && indices.isValid() && extensions_supported && swap_chain_adequate;
}

bool VulkanApp::createLogicalDevice() {
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
    // TODO fill in features

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

bool VulkanApp::createSwapChain() {
    SwapChainSupportDetails swap_chain_support = querySwapChainSupport(physical_device_, window_surface_);

    VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = chooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D extent = chooseSwapExtent(swap_chain_support.capabilities, window_);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;

    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = window_surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
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

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
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
    swap_chain_extent_ = extent;

    return true;
}

bool VulkanApp::createImageViews() {
    swap_chain_image_views_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swap_chain_images_[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swap_chain_image_format_;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &create_info, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image views!" << std::endl;
            return false;
        }
    }

    return true;
}

bool VulkanApp::createCommandPool() {
    QueueFamilyIndices queue_family_indices = findQueueFamilies(physical_device_, window_surface_);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
    pool_info.flags = 0; // Optional

    if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanApp::createCommandBuffers() {
    command_buffers_.resize(swap_chain_image_views_.size());

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = (uint32_t)command_buffers_.size();

    if (vkAllocateCommandBuffers(device_, &alloc_info, command_buffers_.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanApp::createSyncObjects() {
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

VkShaderModule VulkanApp::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module!" << std::endl;
    }

    return shader_module;
}

void VulkanApp::mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        updateScene();
        drawFrame();
    }

    vkDeviceWaitIdle(device_);
}

void VulkanApp::drawFrame() {
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(device_, swap_chain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!recreateSwapChain()) {
            std::cerr << "Failed to re-create swap chain!" << std::endl;
        }
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire swap chain image!" << std::endl;
        return;
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores_[current_frame_] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[image_index];

    VkSemaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    if (vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer!" << std::endl;
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { swap_chain_ };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr; // Optional

    result = vkQueuePresentKHR(present_queue_, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_resized_) {
        window_resized_ = false;
        if (!recreateSwapChain()) {
            std::cerr << "Failed to re-create swap chain!" << std::endl;
        }
    }
    else if (result != VK_SUCCESS) {
        std::cerr << "Failed to present swap chain image!" << std::endl;
    }

    current_frame_ = (current_frame_ + 1) % max_frames_in_flight_;
}

void VulkanApp::cleanup() {
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

    glfwDestroyWindow(window_);
    glfwTerminate();
}

void VulkanApp::cleanupSwapChain() {
    for (auto framebuffer : swap_chain_framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }

    vkFreeCommandBuffers(device_, command_pool_, command_buffers_.size(), command_buffers_.data());
    vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    vkDestroyRenderPass(device_, render_pass_, nullptr);

    for (auto image_view : swap_chain_image_views_) {
        vkDestroyImageView(device_, image_view, nullptr);
    }

    vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
}

bool VulkanApp::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        // window is minimized, paused until it's in foreground again
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapChain();

    if (!createSwapChain()) {
        return false;
    }
    if (!createImageViews()) {
        return false;
    }
    if (!createRenderPass()) {
        return false;
    }
    if (!createGraphicsPipeline()) {
        return false;
    }
    if (!createSwapChainFramebuffers()) {
        return false;
    }
    if (!createCommandBuffers()) {
        return false;
    }

    // re-record the command buffers
    setupScene();

    return true;
}
