// vulkan.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class VulkanTutorial {
public:
    bool setup();
    void run();

private:
    void createWindow();
    bool initVulkan();
    bool createInstance();
    void mainLoop();
    void cleanup();
    void printSupportedExtensions();

    // window
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;

    // vulkan API
    VkInstance vk_instance_;
};