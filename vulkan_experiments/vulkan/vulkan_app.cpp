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

#include <algorithm>

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

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
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
    VkResult result = vulkan_backend_.submitCommands();

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_resized_) {
        window_resized_ = false;
        if (!recreateSwapChain()) {
            std::cerr << "Failed to re-create swap chain!" << std::endl;
        }
    }
}
