/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "vulkan_app.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Additional Helper Functions
namespace {
    void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
        app->onWindowResized();
    }
}

// VulkanApp

bool VulkanApp::setup() {
    createWindow();

    uint32_t glfw_extension_count = 0;
    const char** glfw_required_extensions;

    glfw_required_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    VkInstance instance;
    if (!vulkan_backend_.createInstance(glfw_extension_count, glfw_required_extensions, instance)) {
        return false;
    }

    // setup the window surface 
    VkSurfaceKHR window_surface;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &window_surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!" << std::endl;
        return false;
    }

    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    VkExtent2D window_size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    vulkan_backend_.setWindowSurface(window_surface, window_size);

    if (!vulkan_backend_.startUp()) {
        return false;
    }

    return true;
}

bool VulkanApp::run() {
    if (!loadAssets()) {
        return false;
    }

    if (!setupScene()) {
        return false;
    }

    mainLoop();

    vulkan_backend_.waitDeviceIdle();

    cleanup();

    vulkan_backend_.shutDown();
    glfwDestroyWindow(window_);
    glfwTerminate();

    return true;
}

void VulkanApp::createWindow() {
    glfwInit();

    auto monitor = glfwGetPrimaryMonitor();
    const auto video_mode = glfwGetVideoMode(monitor);
    float width_scale, height_scale;
    glfwGetMonitorContentScale(monitor, &width_scale, &height_scale);
    
    auto monitor_height = video_mode->height * height_scale;
    auto window_width = base_width_;
    auto window_height = base_height_;

    if (monitor_height > 1080.0f) {
        window_width *= 2;
        window_height *= 2;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // on Linux for some reason GLFW_TRUE causes the window to be resized to fullscreen \o/
   
    window_ = glfwCreateWindow(window_width, window_height, window_title_.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

void VulkanApp::mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        updateScene();
        drawFrame();
    }
}

bool VulkanApp::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        // window is minimized, paused until it's in foreground again
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vulkan_backend_.waitDeviceIdle();

    cleanupSwapChainAssets();
    
    vulkan_backend_.resetWindowSwapExtent({ static_cast<uint32_t>(width), static_cast<uint32_t>(height) });

    if (!vulkan_backend_.recreateSwapChain()) {
        return false;
    }

    if (!setupScene()) {
        return false;
    }

    return true;
}

void VulkanApp::drawFrame() {
    uint32_t next_swapchain_image = 0;
    VkResult result = vulkan_backend_.startNextFrame(next_swapchain_image, window_resized_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        window_resized_ = false;
        if (!recreateSwapChain()) {
            std::cerr << "Failed to re-create swap chain!" << std::endl;
        }
        return;
    }

    auto commands = renderFrame(next_swapchain_image);
    auto success = std::get<0>(commands);

    if (success) {
        auto cmd_buffer = std::get<1>(commands);
        result = vulkan_backend_.submitGraphicsCommands(next_swapchain_image, cmd_buffer);
    }

    if (result != VK_SUCCESS) {
        std::cerr << "Submit Commands failed!" << std::endl;
    }
}
